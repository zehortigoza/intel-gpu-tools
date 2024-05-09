// SPDX-License-Identifier: MIT
/*
 * Copyright © 2024 Intel Corporation
 */


#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "intel_chipset.h"
#include "xe/xe_oa.h"

static bool
read_file_uint64(const char *file, uint64_t *value)
{
	char buf[32];
	int fd, n;

	fd = open(file, 0);
	if (fd < 0)
		return false;
	n = read(fd, buf, sizeof (buf) - 1);
	close(fd);
	if (n < 0)
		return false;

	buf[n] = '\0';
	*value = strtoull(buf, 0, 0);

	return true;
}

static uint32_t
read_device_param(const char *stem, int id, const char *param)
{
	char *name;
	int ret = asprintf(&name, "/sys/class/drm/%s%u/device/%s", stem, id, param);
	uint64_t value;
	bool success;

	assert(ret != -1);

	success = read_file_uint64(name, &value);
	free(name);

	return success ? value : 0;
}

static int
find_intel_render_node(void)
{
	for (int i = 128; i < (128 + 16); i++) {
		if (read_device_param("renderD", i, "vendor") == 0x8086)
			return i;
	}

	return -1;
}

static int
open_render_node(uint32_t *devid)
{
	char *name;
	int ret;
	int fd;

	int render = find_intel_render_node();
	if (render < 0)
		return -1;

	ret = asprintf(&name, "/dev/dri/renderD%u", render);
	assert(ret != -1);

	*devid = read_device_param("renderD", render, "device");

	fd = open(name, O_RDWR);
	free(name);

	return fd;
}

static int
get_card_for_fd(int fd)
{
	struct stat sb;
	int mjr, mnr;
	char buffer[128];
	DIR *drm_dir;
	struct dirent *entry;
	int retval = -1;

	if (fstat(fd, &sb)) {
		fprintf(stderr, "Failed to stat DRM fd\n");
		return -1;
	}

	mjr = major(sb.st_rdev);
	mnr = minor(sb.st_rdev);

	snprintf(buffer, sizeof(buffer), "/sys/dev/char/%d:%d/device/drm", mjr, mnr);

	drm_dir = opendir(buffer);
	assert(drm_dir != NULL);

	while ((entry = readdir(drm_dir))) {
		if (entry->d_type == DT_DIR && strncmp(entry->d_name, "card", 4) == 0) {
			retval = strtoull(entry->d_name + 4, NULL, 10);
			break;
		}
	}

	closedir(drm_dir);

	return retval;
}

static const char *
metric_name(struct intel_xe_perf *perf, const char *hw_config_guid)
{
	struct intel_xe_perf_metric_set *metric_set;

	igt_list_for_each_entry(metric_set, &perf->metric_sets, link) {
		if (!strcmp(metric_set->hw_config_guid, hw_config_guid))
			return metric_set->symbol_name;
	}

	return "Unknown";
}

static void
usage(void)
{
	printf("Usage: xe-perf-configs [options]\n"
	       "Manages xe-perf configurations stored in xe.\n"
	       "     --purge, -p         Purge configurations from the kernel\n"
	       "     --list,  -l         List configurations from the kernel\n");
}

int
main(int argc, char *argv[])
{
	char metrics_path[128];
	DIR *metrics_dir;
	struct dirent *entry;
	int drm_fd, drm_card;
	int opt;
	bool purge = false;
	const struct option long_options[] = {
		{"help",   no_argument, 0, 'h'},
		{"list",   no_argument, 0, 'l'},
		{"purge",  no_argument, 0, 'p'},
		{0, 0, 0, 0}
	};
	const struct intel_device_info *devinfo;
	struct intel_xe_perf *perf;
	uint32_t devid = 0;

	while ((opt = getopt_long(argc, argv, "hlp", long_options, NULL)) != -1) {
		switch (opt) {
		case 'h':
			usage();
			return EXIT_SUCCESS;
		case 'l':
			break;
		case 'p':
			purge = true;
			break;
		default:
			fprintf(stderr, "Internal error: "
				"unexpected getopt value: %d\n", opt);
			usage();
			return EXIT_FAILURE;
		}
	}

	drm_fd = open_render_node(&devid);
	drm_card = get_card_for_fd(drm_fd);

	fprintf(stdout, "Found device id=0x%x\n", devid);

	devinfo = intel_get_device_info(drm_fd);
	if (!devinfo) {
		fprintf(stderr, "No device info found.\n");
		return EXIT_FAILURE;
	}

	fprintf(stdout, "Device graphics_ver=%i gt=%i\n", devinfo->graphics_ver, devinfo->gt);

	perf = intel_xe_perf_for_fd(drm_fd, 0);
	if (!perf) {
		fprintf(stderr, "No perf data found.\n");
		return EXIT_FAILURE;
	}

	snprintf(metrics_path, sizeof(metrics_path),
		 "/sys/class/drm/card%d/metrics", drm_card);
	metrics_dir = opendir(metrics_path);
	if (!metrics_dir)
		return EXIT_FAILURE;

	fprintf(stdout, "Looking at metrics in %s\n", metrics_path);

	while ((entry = readdir(metrics_dir))) {
		char metric_id_path[400];
		uint64_t metric_id;

		if (entry->d_type != DT_DIR)
			continue;

		snprintf(metric_id_path, sizeof(metric_id_path),
			 "%s/%s/id", metrics_path, entry->d_name);

		if (!read_file_uint64(metric_id_path, &metric_id))
			continue;

		if (purge) {
			if (intel_xe_perf_ioctl(drm_fd, DRM_XE_PERF_OP_REMOVE_CONFIG, &metric_id) == 0)
				fprintf(stdout, "\tRemoved config %s id=%03" PRIu64 " name=%s\n",
					entry->d_name, metric_id, metric_name(perf, entry->d_name));
			else
				fprintf(stdout, "\tFailed to remove config %s id=%03" PRIu64 " name=%s\n",
					entry->d_name, metric_id, metric_name(perf, entry->d_name));
		} else {
			fprintf(stdout, "\tConfig %s id=%03" PRIu64 " name=%s\n",
				entry->d_name, metric_id, metric_name(perf, entry->d_name));
		}
	}

	closedir(metrics_dir);
	close(drm_fd);

	return EXIT_SUCCESS;
}
