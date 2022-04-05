/*
 * Copyright © 2022 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 */

#include "igt.h"
#include "igt_sysfs.h"
#include "igt_drrs.h"
#include <errno.h>
#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/timerfd.h>

/* TODO: check states */

IGT_TEST_DESCRIPTION("Test do mode switch without modeset or user noticing.");

#define FLIPS_PER_SEC 30
#define CHANGE_REFRESH_RATE_AT_EVEY_X_SEC 5
#define MODESET_AT_EVERY_X_SEC 13
#define COMPLETE_TEST_IN_X_SEC (60 * 60)

enum drrs_mode {
	DRRS_HIGH = 0,
	DRRS_LOW
};

typedef struct {
	int drm_fd;
	int debugfs_fd;
	struct buf_ops *bops;

	igt_display_t display;
	drmModeModeInfo *mode;
	drmModeModeInfo *mode_low;
	igt_output_t *output;

	struct igt_fb fb[60];
	uint8_t flip_fb_in_use;

	int flip_timerfd;
	int modeset_timerfd;
	int switch_refresh_rate_timerfd;
	int complete_timerfd;

	enum drrs_mode current_drrs_mode;
} data_t;

static void setup_output(data_t *data)
{
	igt_display_t *display = &data->display;
	igt_output_t *output;
	enum pipe pipe;
	int i;

	igt_display_require(&data->display, data->drm_fd);

	for_each_pipe_with_valid_output(display, pipe, output) {
		drmModeConnectorPtr c = output->config.connector;

		if (c->connector_type != DRM_MODE_CONNECTOR_eDP)
			continue;

		igt_output_set_pipe(output, pipe);
		data->output = output;
		data->mode = igt_output_get_mode(output);

		igt_info("mode vrefresh=%i name=%s\n",
			 data->mode->vrefresh, data->mode->name);
		kmstest_dump_mode(data->mode);

		break;
	}

	igt_require(data->output);

	/* Search for a low refresh rate mode */
	for (i = 0; i < data->output->config.connector->count_modes; i++) {
		drmModeModeInfo *m = &data->output->config.connector->modes[i];

		if (m->hdisplay != data->mode->hdisplay ||
		    m->vdisplay != data->mode->vdisplay ||
		    m->hsync_start != data->mode->hsync_start ||
		    m->hsync_end != data->mode->hsync_end ||
		    m->vsync_start != data->mode->vsync_start ||
		    m->vsync_end != data->mode->vsync_end ||
		    m->flags != data->mode->flags)
		    continue;

		if (m->vrefresh >= data->mode->vrefresh)
			continue;

		igt_info("low refresh rate mode found vrefresh=%i name=%s\n",
			 m->vrefresh, m->name);
		kmstest_dump_mode(m);
		data->mode_low = m;
	}

	igt_require(data->mode_low);
}

static void prepare(data_t *data)
{
	struct itimerspec interval;
	igt_plane_t *primary;
	int i;

	for (i = 0; i < ARRAY_SIZE(data->fb); i++) {
		uint32_t cl;
		int w;

		/* paint in green a box */
		cl = (0x0 << 0) | (0xFF << 8) | (0x0 << 16) | (0xFF << 24);
		w = data->mode->hdisplay / ARRAY_SIZE(data->fb);
		w *= i;

		igt_create_color_fb(data->drm_fd,
				    data->mode->hdisplay, data->mode->vdisplay,
				    DRM_FORMAT_XRGB8888,
				    DRM_FORMAT_MOD_LINEAR,
				    1.0, 1.0, 1.0,
				    &data->fb[i]);

		igt_draw_rect_fb(data->drm_fd, data->bops, 0,
				 &data->fb[i], IGT_DRAW_BLT, 0, 300, w, 500, cl);
	}

	primary = igt_output_get_plane_type(data->output,
					    DRM_PLANE_TYPE_PRIMARY);
	data->flip_fb_in_use = 0;
	igt_plane_set_fb(primary, &data->fb[0]);
	igt_display_commit2(&data->display, COMMIT_ATOMIC);

	interval.it_value.tv_nsec = 0;
	interval.it_value.tv_sec = CHANGE_REFRESH_RATE_AT_EVEY_X_SEC;
	interval.it_interval.tv_nsec = interval.it_value.tv_nsec;
	interval.it_interval.tv_sec = interval.it_value.tv_sec;
	i = timerfd_settime(data->switch_refresh_rate_timerfd, 0, &interval, NULL);
	igt_require_f(i != -1, "Error setting switch_refresh_rate_timerfd\n");

	interval.it_value.tv_nsec = NSEC_PER_SEC / FLIPS_PER_SEC;
	interval.it_value.tv_sec = 0;
	interval.it_interval.tv_nsec = interval.it_value.tv_nsec;
	interval.it_interval.tv_sec = interval.it_value.tv_sec;
	i = timerfd_settime(data->flip_timerfd, 0, &interval, NULL);
	igt_require_f(i != -1, "Error setting flip_timerfd\n");

	interval.it_value.tv_nsec = 0;
	interval.it_value.tv_sec = MODESET_AT_EVERY_X_SEC;
	interval.it_interval.tv_nsec = interval.it_value.tv_nsec;
	interval.it_interval.tv_sec = interval.it_value.tv_sec;
	i = timerfd_settime(data->modeset_timerfd, 0, &interval, NULL);
	igt_require_f(i != -1, "Error setting modeset_timerfd\n");

	interval.it_value.tv_nsec = 0;
	interval.it_value.tv_sec = COMPLETE_TEST_IN_X_SEC;
	interval.it_interval.tv_nsec = interval.it_value.tv_nsec;
	interval.it_interval.tv_sec = interval.it_value.tv_sec;
	i = timerfd_settime(data->complete_timerfd, 0, &interval, NULL);
	igt_require_f(i != -1, "Error setting complete_timerfd\n");
}

/*
 * Check if expected refresh rate matches with the one printed in
 * i915_display_info
 */
static void display_info_check(data_t *data)
{
	char buf[1024], search[16], *ch = buf;
	drmModeModeInfo *mode;
	int ret;

	mode = data->current_drrs_mode == DRRS_HIGH ? data->mode : data->mode_low;

	ret = igt_debugfs_simple_read(data->debugfs_fd, "i915_display_info", buf,
				      sizeof(buf));
	if (ret < 0) {
		igt_info("Could not read i915_display_info: %s\n",
			 strerror(-ret));
		return;
	}

	/* point ch to pipe used in the test */
	snprintf(search, sizeof(search), ":pipe %s]:",
		 kmstest_pipe_name(data->output->pending_pipe));
	ch = strstr(ch, search);
	igt_assert(ch);

	/* test if pipe is enabled and active */
	ch = strstr(ch, "enable=yes, active=yes, mode=");
	igt_assert(ch);

	/* compare if refresh rate matches with expected */
	snprintf(search, sizeof(search), "\": %i", mode->vrefresh);
	ch = strstr(ch, search);
	igt_assert(ch);
}

static void switch_refresh_rate(data_t *data)
{
	drmModeModeInfo *mode;
	int ret;

	data->current_drrs_mode = !data->current_drrs_mode;

	/* TODO remove it */
	igt_kmsg(KMSG_INFO "switch_refresh_rate() mode=%s\n",
		 data->current_drrs_mode == DRRS_HIGH ? "high" : "low");

	mode = data->current_drrs_mode == DRRS_HIGH ? data->mode : data->mode_low;
	igt_output_override_mode(data->output, mode);
	/* IMPORTANT: no DRM_MODE_ATOMIC_ALLOW_MODESET flag set */
	ret = igt_display_try_commit_atomic(&data->display, 0, NULL);
	igt_assert(ret == 0);

	igt_info("Switched to %s refresh rate mode.\n",
		 data->current_drrs_mode == DRRS_HIGH ? "high" : "low");

	igt_assert(drrs_is_active(data->debugfs_fd));
	igt_assert(drrs_is_low_refresh_rate(data->debugfs_fd) ==
		   data->current_drrs_mode);
	display_info_check(data);
}

static void modeset(data_t *data)
{
	drmModeModeInfo *mode;
	int ret;

	if (data->current_drrs_mode == DRRS_HIGH) {
		igt_info("Skipping modeset because a modeset to low refrersh rate mode would disable seamless DRRS\n");
		return;
	}

	data->current_drrs_mode = !data->current_drrs_mode;

	/* TODO remove it */
	igt_kmsg(KMSG_INFO "modeset mode=%s\n",
		 data->current_drrs_mode == DRRS_HIGH ? "high" : "low");

	mode = data->current_drrs_mode == DRRS_HIGH ? data->mode : data->mode_low;
	igt_output_override_mode(data->output, mode);
	ret = igt_display_try_commit_atomic(&data->display, DRM_MODE_ATOMIC_ALLOW_MODESET, NULL);
	igt_assert(ret == 0);

	igt_info("Modeset to high refresh rate mode.\n");

	igt_assert(drrs_is_active(data->debugfs_fd));
	igt_assert(drrs_is_low_refresh_rate(data->debugfs_fd) ==
		   data->current_drrs_mode);
	display_info_check(data);
}

static void flip(data_t *data)
{
	igt_plane_t *plane;
	struct igt_fb *fb;
	uint8_t next;

	next = data->flip_fb_in_use + 1;
	if (next == ARRAY_SIZE(data->fb))
		next = 0;

	plane = igt_output_get_plane_type(data->output, DRM_PLANE_TYPE_PRIMARY);
	fb = &data->fb[next];

	igt_plane_set_fb(plane, fb);
	igt_display_commit2(&data->display, COMMIT_ATOMIC);
	data->flip_fb_in_use = next;
}

static void run(data_t *data)
{
	struct pollfd pfd[4];
	bool loop = true;

	pfd[0].fd = data->switch_refresh_rate_timerfd;
	pfd[0].events = POLLIN;
	pfd[0].revents = 0;

	pfd[1].fd = data->flip_timerfd;
	pfd[1].events = POLLIN;
	pfd[1].revents = 0;

	pfd[2].fd = data->complete_timerfd;
	pfd[2].events = POLLIN;
	pfd[2].revents = 0;

	pfd[3].fd = data->modeset_timerfd;
	pfd[3].events = POLLIN;
	pfd[3].revents = 0;

	while (loop) {
		int i, r = poll(pfd, ARRAY_SIZE(pfd), -1);

		if (r < 0)
			break;
		if (r == 0)
			continue;

		for (i = 0; i < ARRAY_SIZE(pfd); i++) {
			uint64_t exp;

			if (pfd[i].revents == 0)
				continue;

			pfd[i].revents = 0;
			r = read(pfd[i].fd, &exp, sizeof(exp));
			if (r != sizeof(uint64_t) || exp == 0)
				continue;

			if (pfd[i].fd == data->switch_refresh_rate_timerfd)
				switch_refresh_rate(data);
			else if (pfd[i].fd == data->flip_timerfd)
				flip(data);
			else if (pfd[i].fd == data->complete_timerfd)
				loop = false;
			else if (pfd[i].fd == data->modeset_timerfd)
				modeset(data);
		}
	}
}

static void cleanup(data_t *data)
{
	struct itimerspec interval;
	igt_plane_t *primary;
	int i;

	primary = igt_output_get_plane_type(data->output,
					    DRM_PLANE_TYPE_PRIMARY);
	igt_plane_set_fb(primary, NULL);
	igt_display_commit2(&data->display, COMMIT_ATOMIC);

	for (i = 0; i < ARRAY_SIZE(data->fb); i++)
		igt_remove_fb(data->drm_fd, &data->fb[i]);

	/* disarm timers */
	interval.it_value.tv_nsec = 0;
	interval.it_value.tv_sec = 0;
	interval.it_interval.tv_nsec = interval.it_value.tv_nsec;
	interval.it_interval.tv_sec = interval.it_value.tv_sec;
	timerfd_settime(data->switch_refresh_rate_timerfd, 0, &interval, NULL);
	timerfd_settime(data->flip_timerfd, 0, &interval, NULL);
	timerfd_settime(data->complete_timerfd, 0, &interval, NULL);
	timerfd_settime(data->modeset_timerfd, 0, &interval, NULL);
}

igt_main
{
	data_t data = {};

	igt_fixture {
		data.drm_fd = drm_open_driver_master(DRIVER_INTEL);
		data.debugfs_fd = igt_debugfs_dir(data.drm_fd);
		data.bops = buf_ops_create(data.drm_fd);
		kmstest_set_vt_graphics_mode();

		setup_output(&data);

		igt_require(drrs_is_enabled(data.debugfs_fd));

		data.switch_refresh_rate_timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
		igt_require(data.switch_refresh_rate_timerfd != -1);

		data.flip_timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
		igt_require(data.flip_timerfd != -1);

		data.complete_timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
		igt_require(data.complete_timerfd != -1);

		data.modeset_timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
		igt_require(data.modeset_timerfd != -1);
	}

	igt_describe("Test DRRS switch using modes");
	igt_subtest("basic") {
		data.current_drrs_mode = DRRS_HIGH;
		prepare(&data);
		run(&data);
		cleanup(&data);
	}

	igt_fixture {
		buf_ops_destroy(data.bops);
		igt_display_fini(&data.display);
		close(data.debugfs_fd);
		close(data.drm_fd);
	}
}
