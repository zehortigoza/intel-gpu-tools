// SPDX-License-Identifier: MIT
/*
 * Copyright © 2023 Intel Corporation
 */

#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <termios.h>
#include <sys/sysmacros.h>
#include <stdbool.h>

#include "igt_drm_clients.h"
#include "igt_drm_fdinfo.h"
#include "drmtest.h"

static const char *bars[] = { " ", "▏", "▎", "▍", "▌", "▋", "▊", "▉", "█" };

static void n_spaces(const unsigned int n)
{
	unsigned int i;

	for (i = 0; i < n; i++)
		putchar(' ');
}

static void print_percentage_bar(double percent, int max_len)
{
	int bar_len, i, len = max_len - 1;
	const int w = 8;

	len -= printf("|%5.1f%% ", percent);

	/* no space left for bars, do what we can */
	if (len < 0)
		len = 0;

	bar_len = ceil(w * percent * len / 100.0);
	if (bar_len > w * len)
		bar_len = w * len;

	for (i = bar_len; i >= w; i -= w)
		printf("%s", bars[w]);
	if (i)
		printf("%s", bars[i]);

	len -= (bar_len + (w - 1)) / w;
	n_spaces(len);

	putchar('|');
}

static int
print_client_header(struct igt_drm_client *c, int lines, int con_w, int con_h,
		    int *engine_w)
{
	int ret, len;

	if (lines++ >= con_h)
		return lines;

	printf("\033[7m");
	ret = printf("DRM minor %u", c->drm_minor);
	n_spaces(con_w - ret);

	if (lines++ >= con_h)
		return lines;

	putchar('\n');
	if (c->regions->num_regions)
		len = printf("%*s      MEM      RSS ",
			     c->clients->max_pid_len, "PID");
	else
		len = printf("%*s ", c->clients->max_pid_len, "PID");

	if (c->engines->num_engines) {
		unsigned int i;
		int width;

		*engine_w = width =
			(con_w - len - c->clients->max_name_len - 1) /
			c->engines->num_engines;

		for (i = 0; i <= c->engines->max_engine_id; i++) {
			const char *name = c->engines->names[i];
			int name_len = strlen(name);
			int pad = (width - name_len) / 2;
			int spaces = width - pad - name_len;

			if (!name)
				continue;

			if (pad < 0 || spaces < 0)
				continue;

			n_spaces(pad);
			printf("%s", name);
			n_spaces(spaces);
			len += pad + name_len + spaces;
		}
	}

	printf(" %-*s\033[0m\n", con_w - len - 1, "NAME");

	return lines;
}

static bool
engines_identical(const struct igt_drm_client *c,
		  const struct igt_drm_client *pc)
{
	unsigned int i;

	if (c->engines->num_engines != pc->engines->num_engines ||
	    c->engines->max_engine_id != pc->engines->max_engine_id)
		return false;

	for (i = 0; i <= c->engines->max_engine_id; i++)
		if (c->engines->capacity[i] != pc->engines->capacity[i] ||
		    !!c->engines->names[i] != !!pc->engines->names[i] ||
		    strcmp(c->engines->names[i], pc->engines->names[i]))
			return false;

	return true;
}

static bool
newheader(const struct igt_drm_client *c, const struct igt_drm_client *pc)
{
	return !pc || c->drm_minor != pc->drm_minor ||
	       /*
		* Below is a a hack for drivers like amdgpu which omit listing
		* unused engines. Simply treat them as separate minors which
		* will ensure the per-engine columns are correctly sized in all
		* cases.
		*/
	       !engines_identical(c, pc);
}

static int
print_size(uint64_t sz)
{
	char units[] = {'B', 'K', 'M', 'G'};
	unsigned int u;

	for (u = 0; u < ARRAY_SIZE(units) - 1; u++) {
		if (sz < 1024)
			break;
		sz /= 1024;
	}

	return printf("%7"PRIu64"%c ", sz, units[u]);
}

static int
print_client(struct igt_drm_client *c, struct igt_drm_client **prevc,
	     double t, int lines, int con_w, int con_h,
	     unsigned int period_us, int *engine_w)
{
	unsigned int i;
	uint64_t sz;
	int len;

	/* Filter out idle clients. */
	if (!c->total_runtime || c->samples < 2)
		return lines;

	/* Print header when moving to a different DRM card. */
	if (newheader(c, *prevc)) {
		lines = print_client_header(c, lines, con_w, con_h, engine_w);
		if (lines >= con_h)
			return lines;
	}

	*prevc = c;

	len = printf("%*s ", c->clients->max_pid_len, c->pid_str);

	if (c->regions->num_regions) {
		for (sz = 0, i = 0; i <= c->regions->max_region_id; i++)
			sz += c->memory[i].total;
		len += print_size(sz);

		for (sz = 0, i = 0; i <= c->regions->max_region_id; i++)
			sz += c->memory[i].resident;
		len += print_size(sz);
	}

	lines++;

	for (i = 0; c->samples > 1 && i <= c->engines->max_engine_id; i++) {
		double pct;

		if (!c->engines->capacity[i])
			continue;

		pct = (double)c->val[i] / period_us / 1e3 * 100 /
		      c->engines->capacity[i];

		/*
		 * Guard against fluctuations between our scanning period and
		 * GPU times as exported by the kernel in fdinfo.
		 */
		if (pct > 100.0)
			pct = 100.0;

		print_percentage_bar(pct, *engine_w);
		len += *engine_w;
	}

	printf(" %-*s\n", con_w - len - 1, c->print_name);

	return lines;
}

static int
__client_id_cmp(const struct igt_drm_client *a,
		const struct igt_drm_client *b)
{
	if (a->id > b->id)
		return 1;
	else if (a->id < b->id)
		return -1;
	else
		return 0;
}

static int client_cmp(const void *_a, const void *_b, void *unused)
{
	const struct igt_drm_client *a = _a;
	const struct igt_drm_client *b = _b;
	long val_a, val_b;

	/* DRM cards into consecutive buckets first. */
	val_a = a->drm_minor;
	val_b = b->drm_minor;
	if (val_a > val_b)
		return 1;
	else if (val_b > val_a)
		return -1;

	/*
	 * Within buckets sort by last sampling period aggregated runtime, with
	 * client id as a tie-breaker.
	 */
	val_a = a->last_runtime;
	val_b = b->last_runtime;
	if (val_a == val_b)
		return __client_id_cmp(a, b);
	else if (val_b > val_a)
		return 1;
	else
		return -1;

}

static void update_console_size(int *w, int *h)
{
	struct winsize ws = {};

	if (ioctl(0, TIOCGWINSZ, &ws) == -1)
		return;

	*w = ws.ws_col;
	*h = ws.ws_row;

	if (*w == 0 && *h == 0) {
		/* Serial console. */
		*w = 80;
		*h = 24;
	}
}

static void clrscr(void)
{
	printf("\033[H\033[J");
}

struct gputop_args {
};

static void help(void)
{
	printf("Usage:\n"
	       "\t%s [options]\n\n"
	       "Options:\n"
	       "\t-h, --help                show this help\n"
	       , program_invocation_short_name);
}

static int parse_args(int argc, char * const argv[], struct gputop_args *args)
{
	static const char cmdopts_s[] = "h";
	static const struct option cmdopts[] = {
	       {"help", no_argument, 0, 'h'},
	       { }
	};

	/* defaults */
	memset(args, 0, sizeof(*args));

	for (;;) {
		int c, idx = 0;

		c = getopt_long(argc, argv, cmdopts_s, cmdopts, &idx);
		if (c == -1)
			break;

		switch (c) {
		case 'h':
			help();
			return 0;
		default:
			fprintf(stderr, "Unkonwn option '%c'.\n", c);
			return -1;
		}
	}

	return 1;
}

int main(int argc, char **argv)
{
	struct gputop_args args;
	unsigned int period_us = 2e6;
	struct igt_drm_clients *clients = NULL;
	int con_w = -1, con_h = -1;
	int ret;

	ret = parse_args(argc, argv, &args);
	if (ret < 0)
		return EXIT_FAILURE;
	if (!ret)
		return EXIT_SUCCESS;

	clients = igt_drm_clients_init(NULL);
	if (!clients)
		exit(1);

	igt_drm_clients_scan(clients, NULL, NULL, 0, NULL, 0);

	for (;;) {
		struct igt_drm_client *c, *prevc = NULL;
		int i, engine_w = 0, lines = 0;

		igt_drm_clients_scan(clients, NULL, NULL, 0, NULL, 0);
		igt_drm_clients_sort(clients, client_cmp);

		update_console_size(&con_w, &con_h);
		clrscr();

		igt_for_each_drm_client(clients, c, i) {
			assert(c->status != IGT_DRM_CLIENT_PROBE);
			if (c->status != IGT_DRM_CLIENT_ALIVE)
				break; /* Active clients are first in the array. */

			lines = print_client(c, &prevc, (double)period_us / 1e6,
					     lines, con_w, con_h, period_us,
					     &engine_w);
			if (lines >= con_h)
				break;
		}

		if (lines++ < con_h)
			printf("\n");

		usleep(period_us);
	}

	igt_drm_clients_free(clients);

	return 0;
}
