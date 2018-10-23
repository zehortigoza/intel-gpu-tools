/*
 * Copyright © 2018 Intel Corporation
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
#include "igt_psr.h"
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/timerfd.h>
#include "intel_bufmgr.h"

IGT_TEST_DESCRIPTION("Test PSR2 selective update");

#define SQUARE_SIZE 100

enum operations {
	PAGE_FLIP,
	FRONTBUFFER,
	LAST
};

static const char *op_str(enum operations op)
{
	static const char * const name[] = {
		[PAGE_FLIP] = "page_flip",
		[FRONTBUFFER] = "frontbuffer"
	};

	return name[op];
}

typedef struct {
	struct igt_fb fb[2];
	struct pollfd pollfds[2];
	pthread_t thread_id;
	int drm_fd;
	int debugfs_fd;
	int flip_timerfd;
	int fail_timerfd;
	uint32_t changes;
	volatile bool run;
	volatile bool sucess;

	enum operations op;
	uint32_t devid;
	uint32_t crtc_id;
	igt_display_t display;
	drm_intel_bufmgr *bufmgr;
	int mod_size;
	int mod_stride;
	drmModeModeInfo *mode;
	igt_output_t *output;
} data_t;

static void setup_output(data_t *data)
{
	igt_display_t *display = &data->display;
	igt_output_t *output;
	enum pipe pipe;

	for_each_pipe_with_valid_output(display, pipe, output) {
		drmModeConnectorPtr c = output->config.connector;

		if (c->connector_type != DRM_MODE_CONNECTOR_eDP)
			continue;

		igt_output_set_pipe(output, pipe);
		data->crtc_id = output->config.crtc->crtc_id;
		data->output = output;
		data->mode = igt_output_get_mode(output);

		return;
	}
}

static void display_init(data_t *data)
{
	igt_display_require(&data->display, data->drm_fd);
	setup_output(data);
}

static void display_fini(data_t *data)
{
	igt_display_fini(&data->display);
}

static void *debugfs_thread(void *ptr)
{
	data_t *data = ptr;
	uint16_t expected_num_su_blocks;

	/* each selective update block is 4 lines tall */
	expected_num_su_blocks = SQUARE_SIZE / 4;
	expected_num_su_blocks += SQUARE_SIZE % 4 ? 1 : 0;

	while (data->run) {
		uint16_t num_su_blocks;
		bool r;

		r = psr2_read_last_num_su_blocks_val(data->debugfs_fd,
						     &num_su_blocks);
		if (r && num_su_blocks == expected_num_su_blocks) {
			data->run = false;
			data->sucess = true;
			break;
		}

		usleep(1);
	}

	return NULL;
}

static void prepare(data_t *data)
{
	struct itimerspec interval;
	igt_plane_t *primary;
	int r;

	/* all green frame */
	igt_create_color_fb(data->drm_fd,
			    data->mode->hdisplay, data->mode->vdisplay,
			    DRM_FORMAT_XRGB8888,
			    LOCAL_DRM_FORMAT_MOD_NONE,
			    0.0, 1.0, 0.0,
			    &data->fb[0]);

	if (data->op == PAGE_FLIP) {
		cairo_t *cr;

		igt_create_color_fb(data->drm_fd,
				    data->mode->hdisplay, data->mode->vdisplay,
				    DRM_FORMAT_XRGB8888,
				    LOCAL_DRM_FORMAT_MOD_NONE,
				    0.0, 1.0, 0.0,
				    &data->fb[1]);

		cr = igt_get_cairo_ctx(data->drm_fd, &data->fb[1]);
		/* a white square */
		igt_paint_color_alpha(cr, 0, 0, SQUARE_SIZE, SQUARE_SIZE,
				      1.0, 1.0, 1.0, 1.0);
		igt_put_cairo_ctx(data->drm_fd,  &data->fb[1], cr);
	}

	primary = igt_output_get_plane_type(data->output,
					    DRM_PLANE_TYPE_PRIMARY);
	igt_plane_set_fb(primary, NULL);

	igt_display_commit(&data->display);
	igt_plane_set_fb(primary, &data->fb[0]);
	igt_display_commit(&data->display);

	igt_assert(psr2_wait_deep_sleep(data->debugfs_fd));

	data->run = true;
	data->sucess = false;
	data->changes = 0;

	r = pthread_create(&data->thread_id, NULL, debugfs_thread, data);
	if (r)
		igt_warn("Error starting thread: %i\n", r);

	interval.it_value.tv_nsec = 0;
	interval.it_value.tv_sec = 3;
	interval.it_interval.tv_nsec = interval.it_value.tv_nsec;
	interval.it_interval.tv_sec = interval.it_value.tv_sec;
	r = timerfd_settime(data->fail_timerfd, 0, &interval, NULL);
	igt_require_f(r != -1, "Error setting timerfd\n");
}

static void update_screen(data_t *data)
{
	data->changes++;

	switch (data->op) {
	case PAGE_FLIP: {
		igt_plane_t *primary;

		primary = igt_output_get_plane_type(data->output,
						    DRM_PLANE_TYPE_PRIMARY);

		igt_plane_set_fb(primary, &data->fb[data->changes & 1]);
		igt_display_commit(&data->display);
		break;
	}
	case FRONTBUFFER: {
		drmModeClip clip;
		cairo_t *cr;
		int r;

		clip.x1 = clip.y1 = 0;
		clip.x2 = clip.y2 = SQUARE_SIZE;

		cr = igt_get_cairo_ctx(data->drm_fd, &data->fb[0]);

		if (data->changes & 1) {
			/* go back to all green frame with with square */
			igt_paint_color_alpha(cr, 0, 0, SQUARE_SIZE,
					      SQUARE_SIZE, 1.0, 1.0, 1.0, 1.0);
		} else {
			/* go back to all green frame */
			igt_paint_color_alpha(cr, 0, 0, SQUARE_SIZE,
					      SQUARE_SIZE, 0, 1.0, 0, 1.0);
		}

		r = drmModeDirtyFB(data->drm_fd, data->fb[0].fb_id, &clip, 1);
		igt_assert(r == 0 || r == -ENOSYS);
		break;
	}
	default:
		igt_assert_f(data->op, "Operation not handled\n");
	}
}

static void run(data_t *data)
{
	while (data->run) {
		uint64_t exp;
		int r;

		r = poll(data->pollfds,
			 sizeof(data->pollfds) / sizeof(data->pollfds[0]), -1);
		if (r < 0)
			break;

		/* flip_timerfd timeout */
		if (data->pollfds[0].revents & POLLIN) {
			r = read(data->pollfds[0].fd, &exp, sizeof(exp));

			if (r != sizeof(uint64_t)) {
				igt_warn("read a not expected number of bytes from flip_timerfd: %i\n", r);
			} else if (exp)
				update_screen(data);
		}

		/* fail_timerfd timeout */
		if (data->pollfds[1].revents & POLLIN) {
			r = read(data->pollfds[1].fd, &exp, sizeof(exp));

			if (r != sizeof(uint64_t)) {
				igt_warn("read a not expected number of bytes from fail_timerfd: %i\n", r);
			} else if (exp)
				break;
		}
	}

	data->run = false;
	pthread_join(data->thread_id, NULL);

	igt_debug("Changes: %u\n", data->changes);
	igt_assert(data->sucess);
}

static void cleanup(data_t *data)
{
	igt_plane_t *primary;

	primary = igt_output_get_plane_type(data->output,
					    DRM_PLANE_TYPE_PRIMARY);
	igt_plane_set_fb(primary, NULL);
	igt_display_commit(&data->display);

	igt_remove_fb(data->drm_fd, &data->fb[0]);
	if (data->op == PAGE_FLIP)
		igt_remove_fb(data->drm_fd, &data->fb[1]);
}

int main(int argc, char *argv[])
{
	data_t data = {};
	enum operations op;

	igt_subtest_init_parse_opts(&argc, argv, "", NULL,
				    NULL, NULL, NULL);
	igt_skip_on_simulation();

	igt_fixture {
		struct itimerspec interval;
		int r;

		data.drm_fd = drm_open_driver_master(DRIVER_INTEL);
		data.debugfs_fd = igt_debugfs_dir(data.drm_fd);
		kmstest_set_vt_graphics_mode();
		data.devid = intel_get_drm_devid(data.drm_fd);

		igt_require_f(psr2_sink_support(data.debugfs_fd),
			      "Sink does not support PSR2\n");

		data.bufmgr = drm_intel_bufmgr_gem_init(data.drm_fd, 4096);
		igt_assert(data.bufmgr);
		drm_intel_bufmgr_gem_enable_reuse(data.bufmgr);

		display_init(&data);

		igt_require(psr2_enable(data.debugfs_fd));
		igt_require(psr2_wait_deep_sleep(data.debugfs_fd));

		data.flip_timerfd = timerfd_create(CLOCK_MONOTONIC,
						   TFD_NONBLOCK);
		igt_require(data.flip_timerfd != -1);
		interval.it_value.tv_nsec = NSEC_PER_SEC / 15;
		interval.it_value.tv_sec = 0;
		interval.it_interval.tv_nsec = interval.it_value.tv_nsec;
		interval.it_interval.tv_sec = interval.it_value.tv_sec;
		r = timerfd_settime(data.flip_timerfd, 0, &interval, NULL);
		igt_require_f(r != -1, "Error setting timerfd\n");

		data.fail_timerfd = timerfd_create(CLOCK_MONOTONIC,
						   TFD_NONBLOCK);
		igt_require(data.fail_timerfd != -1);

		data.pollfds[0].fd = data.flip_timerfd;
		data.pollfds[0].events = POLLIN;
		data.pollfds[0].revents = 0;

		data.pollfds[1].fd = data.fail_timerfd;
		data.pollfds[1].events = POLLIN;
		data.pollfds[1].revents = 0;
	}

	for (op = PAGE_FLIP; op < LAST; op++) {
		igt_subtest_f("%s", op_str(op)) {
			data.op = op;
			prepare(&data);
			run(&data);
			cleanup(&data);
		}
	}

	igt_fixture {
		close(data.debugfs_fd);
		drm_intel_bufmgr_destroy(data.bufmgr);
		display_fini(&data);
	}

	igt_exit();
}
