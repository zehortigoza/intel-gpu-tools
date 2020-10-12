/*
 * Copyright © 2019 Intel Corporation
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
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/timerfd.h>
#include "intel_bufmgr.h"
#include <signal.h>

IGT_TEST_DESCRIPTION("Test PSR2 selective update");

#define SQUARE_SIZE 100
#define CUR_SIZE 128
#define SPRITE_W (SQUARE_SIZE * 2)
#define SPRITE_H (SQUARE_SIZE / 2)

/* each selective update block is 4 lines tall */
#define EXPECTED_NUM_SU_BLOCKS ((SQUARE_SIZE / 4) + (SQUARE_SIZE % 4 ? 1 : 0))

/*
 * Minimum is 15 as the number of frames to active PSR2 could be configured
 * to 15 frames plus a few more in case we miss a selective update between
 * debugfs reads.
 */
#define MAX_SCREEN_CHANGES 20

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
	int drm_fd;
	int debugfs_fd;
	igt_display_t display;
	drm_intel_bufmgr *bufmgr;
	drmModeModeInfo *mode;
	igt_output_t *output;
	struct igt_fb fb[2], cursor_fb, sprite_fb[2];
	cairo_t *cr[2], *sprite_cr;
	struct drm_mode_rect rect_in_fb[2];
	struct drm_mode_rect rect, cursor_rect;
	enum operations op;
	int change_screen_timerfd;
	uint32_t screen_changes;
	bool no_damage_areas;
	bool no_psr2;
	bool diagonal_move;
	bool with_cursor, with_sprite;
	enum psr_mode psr_mode;
	igt_plane_t *primary, *cursor, *sprite;
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

static void prepare(data_t *data)
{
	cairo_t *cr;

	/* all black frame */
	igt_create_color_fb(data->drm_fd,
			    data->mode->hdisplay, data->mode->vdisplay,
			    DRM_FORMAT_XRGB8888,
			    LOCAL_DRM_FORMAT_MOD_NONE,
			    1.0, 1.0, 1.0,
			    &data->fb[0]);
	data->cr[0] = igt_get_cairo_ctx(data->drm_fd, &data->fb[0]);

	if (data->op == PAGE_FLIP) {
		igt_create_color_fb(data->drm_fd,
				    data->mode->hdisplay, data->mode->vdisplay,
				    DRM_FORMAT_XRGB8888,
				    LOCAL_DRM_FORMAT_MOD_NONE,
				    1.0, 1.0, 1.0,
				    &data->fb[1]);

		data->cr[1] = igt_get_cairo_ctx(data->drm_fd, &data->fb[1]);
	}

	/* paint red rect */
	data->rect.x1 = data->rect.y1 = 0;
	data->rect.x2 = data->rect.y2 = SQUARE_SIZE;
	data->rect_in_fb[1] = data->rect_in_fb[0] = data->rect;
	igt_paint_color_alpha(data->cr[0], data->rect.x1, data->rect.y1,
			      SQUARE_SIZE, SQUARE_SIZE, 1.0, 0.0, 0.0, 1.0);

	data->primary = igt_output_get_plane_type(data->output,
						  DRM_PLANE_TYPE_PRIMARY);
	igt_plane_set_fb(data->primary, &data->fb[0]);

	if (data->with_cursor) {
		igt_create_fb(data->drm_fd, CUR_SIZE, CUR_SIZE,
			      DRM_FORMAT_ARGB8888, LOCAL_DRM_FORMAT_MOD_NONE,
			      &data->cursor_fb);
		cr = igt_get_cairo_ctx(data->drm_fd, &data->cursor_fb);
		/* TODO add some alpha to cursor */
		igt_paint_color_alpha(cr, 0, 0, CUR_SIZE, CUR_SIZE, 0.0, 0.0,
				      1.0, 1.0);
		igt_put_cairo_ctx(cr);
		data->cursor_rect.x1 = 0;
		/* To create some overlapping between cursor and primary */
		data->cursor_rect.y1 = SQUARE_SIZE / 2;
		data->cursor_rect.x2 = CUR_SIZE;
		data->cursor_rect.y2 = data->cursor_rect.y1 + CUR_SIZE;
		data->cursor = igt_output_get_plane_type(data->output,
							 DRM_PLANE_TYPE_CURSOR);
		igt_plane_set_fb(data->cursor, &data->cursor_fb);
	}

	/* Sprite don't move, it only changes color */
	if (data->with_sprite) {
		igt_create_color_fb(data->drm_fd,
				    SPRITE_W, SPRITE_H, DRM_FORMAT_XRGB8888,
				    LOCAL_DRM_FORMAT_MOD_NONE, 1.0, 0.0, 0.0,
				    &data->sprite_fb[0]);
		data->sprite = igt_output_get_plane_type(data->output,
							 DRM_PLANE_TYPE_OVERLAY);
		igt_plane_set_fb(data->sprite, &data->sprite_fb[0]);
		igt_plane_set_position(data->sprite, 10, 75);
		data->sprite_cr = igt_get_cairo_ctx(data->drm_fd,
						    &data->sprite_fb[0]);

		if (data->op == PAGE_FLIP) {
			igt_create_color_fb(data->drm_fd,
					    SPRITE_W, SPRITE_H,
					    DRM_FORMAT_XRGB8888,
					    LOCAL_DRM_FORMAT_MOD_NONE, 0.0, 1.0,
					    0.0, &data->sprite_fb[1]);
		}
	}

	igt_display_commit2(&data->display, COMMIT_ATOMIC);

	if (!igt_plane_has_prop(data->primary, IGT_PLANE_DAMAGE_CLIPS)) {
		igt_debug("Plane do not have damage clips property\n");
		data->no_damage_areas = false;
	}
}

static bool update_screen_and_test(data_t *data)
{
	struct drm_mode_rect *rect_in_fb, primary_damage_clips[2];
	uint16_t su_blocks;
	bool ret = false;
	cairo_t *cr;

	switch (data->op) {
	case PAGE_FLIP:
		cr = data->cr[data->screen_changes & 1];
		rect_in_fb = &data->rect_in_fb[data->screen_changes & 1];
		break;
	case FRONTBUFFER:
		cr = data->cr[0];
		rect_in_fb = &data->rect_in_fb[0];
		break;
	default:
		igt_assert_f(data->op, "Operation not handled\n");
	}

	/* erase old red rect */
	igt_paint_color_alpha(cr, rect_in_fb->x1, rect_in_fb->y1,
			      SQUARE_SIZE, SQUARE_SIZE, 1.0, 1.0, 1.0, 1.0);

	primary_damage_clips[0] = data->rect;

	/* move global rect */
	data->rect.x1++;
	data->rect.x2++;
	if (data->diagonal_move) {
		data->rect.y1++;
		data->rect.y2++;
	}
	if (data->rect.x2 > data->mode->hdisplay ||
	    data->rect.y2 > data->mode->vdisplay) {
		data->rect.x1 = data->rect.y1 = 0;
		data->rect.x2 = data->rect.y2 = SQUARE_SIZE;
	}

	*rect_in_fb = data->rect;

	/* paint red rect */
	igt_paint_color_alpha(cr, data->rect.x1, data->rect.y1, SQUARE_SIZE,
			      SQUARE_SIZE, 1.0, 0.0, 0.0, 1.0);
	primary_damage_clips[1] = data->rect;

	if (data->with_cursor) {
		data->cursor_rect.x1--;
		data->cursor_rect.x2--;

		if (data->diagonal_move) {
			data->cursor_rect.y1++;
			data->cursor_rect.y2++;
		}
		if (data->cursor_rect.x1 <= 0 ||
		    data->cursor_rect.y2 > data->mode->vdisplay) {
			data->cursor_rect.x1 = data->mode->hdisplay - SQUARE_SIZE;
			data->cursor_rect.x2 = data->mode->hdisplay;
			data->cursor_rect.y1 = 0;
			data->cursor_rect.y2 = SQUARE_SIZE;
		}

		igt_plane_set_position(data->cursor, data->cursor_rect.x1,
				       data->cursor_rect.y1);
		/*
		 * Not setting damage area of cursor because it is a plane move
		 * so SW tracking should take care, not sure yet what we should
		 * do for frontbuffer tracking
		 */
	}

	if (data->with_sprite) {
		switch (data->op) {
		case PAGE_FLIP:
			igt_plane_set_fb(data->sprite,
					 &data->sprite_fb[data->screen_changes & 1]);
			/*
			 * Not sending damage area on purpose, sw tracking
			 * should mark this whole plane as damaged
			 */
			break;
		case FRONTBUFFER: {
			double r = 0.0, g = 0.0;

			if (data->screen_changes & 1)
				g = 1.0;
			else
				r = 1.0;

			igt_paint_color_alpha(data->sprite_cr, 0, 0,
					      SPRITE_W, SPRITE_H, r, g, 0.0,
					      1.0);
			break;
		}
		default:
			igt_assert_f(data->op, "Operation not handled\n");
		}
	}

	switch (data->op) {
	case PAGE_FLIP:
		if (!data->no_damage_areas) {
			igt_plane_replace_prop_blob(data->primary,
						    IGT_PLANE_DAMAGE_CLIPS,
						    primary_damage_clips,
						    sizeof(primary_damage_clips));
		}

		igt_plane_set_fb(data->primary,
				 &data->fb[data->screen_changes & 1]);
		igt_display_commit2(&data->display, COMMIT_ATOMIC);
		break;
	case FRONTBUFFER: {
		drmModeClip fb_clip[4];
		uint32_t len = 0;

		int i;

		for (i = 0; i < ARRAY_SIZE(primary_damage_clips); i++) {
			fb_clip[len].x1 = primary_damage_clips[i].x1;
			fb_clip[len].x2 = primary_damage_clips[i].x2;
			fb_clip[len].y1 = primary_damage_clips[i].y1;
			fb_clip[len].y2 = primary_damage_clips[i].y2;
			len++;
		}

		if (data->with_sprite) {
			fb_clip[len].x1 = fb_clip[len].y1 = 0;
			fb_clip[len].x2 = SPRITE_W;
			fb_clip[len].y2 = SPRITE_H;
		}

		/* TODO: what to do for frontbuffer? */

		drmModeDirtyFB(data->drm_fd, data->fb[0].fb_id,
			       (drmModeClipPtr)&fb_clip, len);
		break;
	}
	default:
		igt_assert_f(data->op, "Operation not handled\n");
	}

	if (psr2_wait_su(data->debugfs_fd, &su_blocks)) {
		ret = su_blocks == EXPECTED_NUM_SU_BLOCKS;

		if (!ret)
			igt_debug("Not matching SU blocks read: %u\n", su_blocks);
	}

	return ret;
}

static void run(data_t *data)
{
	bool result = false;

	igt_assert(psr_wait_entry(data->debugfs_fd, data->psr_mode));

	for (data->screen_changes = 1;
	     data->screen_changes < MAX_SCREEN_CHANGES && !result;
	     data->screen_changes++) {
		uint64_t exp;
		int r;

		r = read(data->change_screen_timerfd, &exp, sizeof(exp));
		if (r == sizeof(uint64_t) && exp)
			result = update_screen_and_test(data);
	}

	igt_assert_f(result,
		     "No matching selective update blocks read from debugfs\n");
}

static void cleanup(data_t *data)
{
	if (data->with_cursor)
		igt_plane_set_fb(data->cursor, NULL);
	if (data->with_sprite)
		igt_plane_set_fb(data->sprite, NULL);
	igt_plane_set_fb(data->primary, NULL);
	igt_display_commit2(&data->display, COMMIT_ATOMIC);

	if (data->op == PAGE_FLIP) {
		igt_put_cairo_ctx(data->cr[1]);
		igt_remove_fb(data->drm_fd, &data->fb[1]);
	}

	if (data->with_cursor)
		igt_remove_fb(data->drm_fd, &data->cursor_fb);
	if (data->with_sprite) {
		igt_put_cairo_ctx(data->sprite_cr);
		igt_remove_fb(data->drm_fd, &data->sprite_fb[0]);
		if (data->op == PAGE_FLIP)
			igt_remove_fb(data->drm_fd, &data->sprite_fb[1]);
	}
	igt_put_cairo_ctx(data->cr[0]);
	igt_remove_fb(data->drm_fd, &data->fb[0]);
}

static bool run_loop = true;

static void sig_term_handler(int signum, siginfo_t *info, void *ptr)
{
	printf("Stopping\n");
	run_loop = false;
}

static void catch_sigterm(void)
{
    static struct sigaction _sigact;

    memset(&_sigact, 0, sizeof(_sigact));
    _sigact.sa_sigaction = sig_term_handler;
    _sigact.sa_flags = SA_SIGINFO;

    sigaction(SIGTERM, &_sigact, NULL);
}

const char *help_str =
	"  --no-damage-areas\tDo not send damage areas.\n"
	"  --no-psr2\tDisable PSR2.\n"
	"  --move-in-xy\tMove the rect in diagonal\n"
	"  --with-cursor\tWith cursor plane\n"
	"  --with-sprite\tWith sprite plane\n";
static struct option long_options[] = {
	{"no-damage-areas", 0, 0, 'n'},
	{"no-psr2", 0, 0, 'p'},
	{"move-in-xy", 0, 0, 'x'},
	{"with-cursor", 0, 0, 'c'},
	{"with-sprite", 0, 0, 's'},
	{ 0, 0, 0, 0 }
};

static int opt_handler(int opt, int opt_index, void *_data)
{
	data_t *data = _data;

	switch (opt) {
	case 'n':
		data->no_damage_areas = true;
		break;
	case 'p':
		data->no_psr2 = true;
		break;
	case 'x':
		data->diagonal_move = true;
		break;
	case 'c':
		data->with_cursor = true;
		break;
	case 's':
		data->with_sprite = true;
		break;
	default:
		return IGT_OPT_HANDLER_ERROR;
	}

	return IGT_OPT_HANDLER_SUCCESS;
}

static data_t g_data = {};

igt_main_args("", long_options, help_str, opt_handler, &g_data)
{
	data_t *data = &g_data;

	igt_fixture {
		struct itimerspec interval;
		int r;

		data->drm_fd = drm_open_driver_master(DRIVER_INTEL);
		data->debugfs_fd = igt_debugfs_dir(data->drm_fd);
		kmstest_set_vt_graphics_mode();

		igt_require_f(psr_sink_support(data->drm_fd, data->debugfs_fd, data->psr_mode),
			      "Sink does not support PSR2\n");

		data->bufmgr = drm_intel_bufmgr_gem_init(data->drm_fd, 4096);
		igt_assert(data->bufmgr);
		drm_intel_bufmgr_gem_enable_reuse(data->bufmgr);

		display_init(data);

		data->psr_mode = data->no_psr2 ? PSR_MODE_1 : PSR_MODE_2;

		/* Test if PSR2 can be enabled */
		igt_require_f(psr_enable(data->drm_fd, data->debugfs_fd, data->psr_mode),
			      "Error enabling PSR\n");
		data->op = FRONTBUFFER;
		prepare(data);
		r = psr_wait_entry(data->debugfs_fd, data->psr_mode);
		cleanup(data);
		igt_require_f(r, "PSR can not be enabled\n");

		/* blocking timerfd */
		data->change_screen_timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
		igt_require(data->change_screen_timerfd != -1);
		/* Changing screen at 30hz to support 30hz panels */
		interval.it_value.tv_nsec = NSEC_PER_SEC / 10;
		interval.it_value.tv_sec = 0;
		interval.it_interval.tv_nsec = interval.it_value.tv_nsec;
		interval.it_interval.tv_sec = interval.it_value.tv_sec;
		r = timerfd_settime(data->change_screen_timerfd, 0, &interval, NULL);
		igt_require_f(r != -1, "Error setting timerfd\n");
	}

	for (data->op = PAGE_FLIP; data->op < LAST; data->op++) {
		igt_subtest_f("%s", op_str(data->op)) {
			prepare(data);
			run(data);
			cleanup(data);
		}
	}

	igt_subtest("psr2-fw-tracking") {
		uint64_t exp;
		int r = 0;

		data->op = PAGE_FLIP;
		prepare(data);

		catch_sigterm();

		igt_assert(psr_wait_entry(data->debugfs_fd, data->psr_mode));

		while (run_loop) {
			r = read(data->change_screen_timerfd, &exp, sizeof(exp));
			data->screen_changes = !data->screen_changes;

			if (r != sizeof(uint64_t) || !exp)
				break;

			if (update_screen_and_test(data))
				update_screen_and_test(data);
			igt_debug_manual_check("all", "flip");
		}

		cleanup(data);
	}

	igt_fixture {
		close(data->debugfs_fd);
		drm_intel_bufmgr_destroy(data->bufmgr);
		display_fini(data);
	}
}
