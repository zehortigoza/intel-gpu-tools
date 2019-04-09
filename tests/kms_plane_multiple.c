/*
 * Copyright © 2016 Intel Corporation
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
#include "igt_rand.h"
#include "drmtest.h"
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

IGT_TEST_DESCRIPTION("Test atomic mode setting with multiple planes ");

#define SIZE_PLANE      256
#define SIZE_CURSOR     128
#define LOOP_FOREVER     -1

typedef struct {
	float red;
	float green;
	float blue;
} color_t;

struct plane_data {
	igt_plane_t *plane;
	struct igt_fb fb;
	bool enabled;
};

typedef struct {
	int drm_fd;
	igt_display_t display;
	igt_crc_t ref_crc;
	igt_pipe_crc_t *pipe_crc;
	struct igt_fb all_blue_primary_fb;
	struct plane_data *planes;
} data_t;

/* Command line parameters. */
struct {
	int iterations;
	bool user_seed;
	int seed;
} opt = {
	.iterations = 1,
	.user_seed = false,
	.seed = 1,
};

/*
 * Common code across all tests, acting on data_t
 */
static void test_init(data_t *data, enum pipe pipe)
{
	igt_plane_t *plane;
	const int n_planes = data->display.pipes[pipe].n_planes;

	data->pipe_crc = igt_pipe_crc_new(data->drm_fd, pipe, INTEL_PIPE_CRC_SOURCE_AUTO);

	data->planes = calloc(n_planes, sizeof(*data->planes));
	igt_assert_f(data->planes != NULL, "Failed to allocate memory for planes\n");

	for_each_plane_on_pipe(&data->display, pipe, plane)
		data->planes[plane->index].plane = plane;
}

static void cleanup_planes(data_t *data, enum pipe pipe)
{
	igt_plane_t *plane;

	for_each_plane_on_pipe(&data->display, pipe, plane) {
		unsigned index = plane->index;

		if (!data->planes[index].enabled)
			continue;

		igt_plane_set_fb(plane, NULL);
		igt_remove_fb(data->drm_fd, &data->planes[index].fb);
		data->planes[index].enabled = false;
	}
}

static void test_fini(data_t *data, igt_output_t *output, enum pipe pipe)
{
	igt_pipe_crc_stop(data->pipe_crc);

	/* reset the constraint on the pipe */
	igt_output_set_pipe(output, PIPE_ANY);

	igt_pipe_crc_free(data->pipe_crc);
	data->pipe_crc = NULL;

	igt_remove_fb(data->drm_fd, &data->all_blue_primary_fb);

	cleanup_planes(data, pipe);
	free(data->planes);
	data->planes = NULL;

	igt_display_reset(&data->display);
}

static void
test_grab_crc(data_t *data, igt_output_t *output, enum pipe pipe,
	      const color_t *color, uint64_t tiling)
{
	drmModeModeInfo *mode;
	igt_plane_t *primary;
	int ret;

	igt_output_set_pipe(output, pipe);

	primary = igt_output_get_plane_type(output, DRM_PLANE_TYPE_PRIMARY);

	mode = igt_output_get_mode(output);

	igt_create_color_fb(data->drm_fd, mode->hdisplay, mode->vdisplay,
			    DRM_FORMAT_XRGB8888,
			    LOCAL_DRM_FORMAT_MOD_NONE,
			    color->red, color->green, color->blue,
			    &data->all_blue_primary_fb);

	igt_plane_set_fb(primary, &data->all_blue_primary_fb);

	ret = igt_display_try_commit2(&data->display, COMMIT_ATOMIC);
	igt_skip_on(ret != 0);

	igt_pipe_crc_start(data->pipe_crc);
	igt_pipe_crc_get_single(data->pipe_crc, &data->ref_crc);
}

/*
 * Multiple plane position test.
 *   - We start by grabbing a reference CRC of a full blue fb being scanned
 *     out on the primary plane
 *   - Then we scannout number of planes:
 *      * the primary plane uses a blue fb with a black rectangle hole
 *      * planes, on top of the primary plane, with a blue fb that is set-up
 *        to cover the black rectangles of the primary plane fb
 *     The resulting CRC should be identical to the reference CRC
 */

static void
create_fb_for_mode_position(data_t *data, enum pipe pipe_id,
			    igt_output_t *output, drmModeModeInfo *mode,
			    const color_t *color, int *rect_x, int *rect_y,
			    int *rect_w, int *rect_h, uint64_t tiling)
{
	unsigned int fb_id;
	cairo_t *cr;
	igt_plane_t *primary, *plane;

	primary = igt_output_get_plane_type(output, DRM_PLANE_TYPE_PRIMARY);

	igt_skip_on(!igt_display_has_format_mod(&data->display,
						DRM_FORMAT_XRGB8888,
						tiling));

	fb_id = igt_create_fb(data->drm_fd,
			      mode->hdisplay, mode->vdisplay,
			      DRM_FORMAT_XRGB8888,
			      tiling,
			      &data->planes[primary->index].fb);
	igt_assert(fb_id);

	cr = igt_get_cairo_ctx(data->drm_fd, &data->planes[primary->index].fb);
	igt_paint_color(cr, 0, 0, mode->hdisplay, mode->vdisplay, color->red,
			color->green, color->blue);

	for_each_plane_on_pipe(&data->display, pipe_id, plane) {
		unsigned i = plane->index;

		if (data->planes[i].plane->type == DRM_PLANE_TYPE_PRIMARY ||
		    !data->planes[i].enabled)
			continue;

		igt_paint_color(cr, rect_x[i], rect_y[i], rect_w[i], rect_h[i],
				0.0, 0.0, 0.0);
	}

	igt_put_cairo_ctx(data->drm_fd, &data->planes[primary->index].fb, cr);
}

static uint32_t plane_format_get(int type)
{
	if (type == DRM_PLANE_TYPE_CURSOR)
		return DRM_FORMAT_ARGB8888;

	return DRM_FORMAT_XRGB8888;
}

static uint64_t plane_tiling_get(int type, uint64_t tiling)
{
	if (type == DRM_PLANE_TYPE_CURSOR)
		return LOCAL_DRM_FORMAT_MOD_NONE;

	return tiling;
}

static int plane_size_get(int type)
{
	if (type == DRM_PLANE_TYPE_CURSOR)
		return SIZE_CURSOR;

	return SIZE_PLANE;
}

static void
prepare_planes(data_t *data, enum pipe pipe_id, const color_t *color,
	       uint64_t tiling, igt_output_t *output, int max_planes)
{
	const int pipe_n_planes = data->display.pipes[pipe_id].n_planes;
	int x[pipe_n_planes], y[pipe_n_planes], size[pipe_n_planes];
	drmModeModeInfo *mode;
	igt_plane_t *primary;
	igt_plane_t *plane;
	int plane_count;

	igt_output_set_pipe(output, pipe_id);
	primary = igt_output_get_plane_type(output, DRM_PLANE_TYPE_PRIMARY);
	mode = igt_output_get_mode(output);

	/* Randomize planes position */
	for_each_plane_on_pipe(&data->display, pipe_id, plane) {
		unsigned i = plane->index, type;
		uint32_t plane_format;
		uint64_t plane_tiling;

		if (plane->type == DRM_PLANE_TYPE_PRIMARY)
			continue;

		type = data->planes[i].plane->type;
		size[i] = plane_size_get(type);
		plane_format = plane_format_get(type);
		plane_tiling = plane_tiling_get(type, tiling);

		igt_skip_on(!igt_plane_has_format_mod(data->planes[i].plane,
						      plane_format ,
						      plane_tiling));

		x[i] = rand() % (mode->hdisplay - size[i]);
		y[i] = rand() % (mode->vdisplay - size[i]);

		data->planes[i].enabled = true;
	}

	/* Limit the number of planes */
	for (plane_count = pipe_n_planes; plane_count > max_planes; ) {
		unsigned i = hars_petruska_f54_1_random_unsafe_max(pipe_n_planes);

		if (data->planes[i].plane->type == DRM_PLANE_TYPE_PRIMARY ||
		    !data->planes[i].enabled)
			continue;

		data->planes[i].enabled = false;
		plane_count--;
	}

	/* Allocate the framebuffers of the enabled planes */
	for_each_plane_on_pipe(&data->display, pipe_id, plane) {
		unsigned i = plane->index, type;
		uint32_t plane_format;
		uint64_t plane_tiling;

		if (data->planes[i].plane->type == DRM_PLANE_TYPE_PRIMARY ||
		    !data->planes[i].enabled)
			continue;

		type = data->planes[i].plane->type;
		plane_format = plane_format_get(type);
		plane_tiling = plane_tiling_get(type, tiling);

		igt_create_color_fb(data->drm_fd, size[i], size[i],
				    plane_format, plane_tiling,
				    color->red, color->green, color->blue,
				    &data->planes[i].fb);

		igt_plane_set_position(data->planes[i].plane, x[i], y[i]);
		igt_plane_set_fb(data->planes[i].plane, &data->planes[i].fb);
	}

	/* primary plane */
	create_fb_for_mode_position(data, pipe_id, output, mode, color, x, y,
				    size, size, tiling);
	igt_plane_set_fb(data->planes[primary->index].plane,
			 &data->planes[primary->index].fb);
	data->planes[primary->index].enabled = true;
}

static void
test_plane_position_with_output(data_t *data, enum pipe pipe,
				igt_output_t *output, uint64_t tiling)
{
	const color_t blue = { 0.0f, 0.0f, 1.0f };
	igt_crc_t crc;
	igt_plane_t *plane;
	int i;
	int iterations = opt.iterations < 1 ? 1 : opt.iterations;
	bool loop_forever;
	char info[256];
	int n_planes = data->display.pipes[pipe].n_planes;

	if (opt.iterations == LOOP_FOREVER) {
		loop_forever = true;
		sprintf(info, "forever");
	} else {
		loop_forever = false;
		sprintf(info, "for %d %s",
			iterations, iterations > 1 ? "iterations" : "iteration");
	}

	test_init(data, pipe);

	test_grab_crc(data, output, pipe, &blue, tiling);

	/*
	 * Find out how many planes are allowed simultaneously
	 */
	while (1) {
		int err;

		prepare_planes(data, pipe, &blue, tiling, output, n_planes);

		/*err = igt_display_try_commit_atomic(&data->display,
						    DRM_MODE_ATOMIC_TEST_ONLY |
						    DRM_MODE_ATOMIC_ALLOW_MODESET,
						    NULL);*/
		err = igt_display_try_commit2(&data->display, COMMIT_ATOMIC);
		igt_warn("err=%i n_planes=%i\n", err, n_planes);

		cleanup_planes(data, pipe);

		if (!err)
			break;

		if (err == -EINVAL) {
			n_planes--;
			igt_assert_f(n_planes > 2, "Unable to enable 2 planes simultaneously\n");
			continue;
		}

		igt_assert_f(!err, "Error %i not expected by try_commit()\n", err);
	}

	igt_info("Testing connector %s using pipe %s with %d planes %s with seed %d\n",
		 igt_output_name(output), kmstest_pipe_name(pipe), n_planes,
		 info, opt.seed);

	for (i = 0; i < iterations || loop_forever; i++) {
		prepare_planes(data, pipe, &blue, tiling, output, n_planes);

		igt_display_commit2(&data->display, COMMIT_ATOMIC);

		igt_pipe_crc_get_current(data->display.drm_fd, data->pipe_crc, &crc);

		for_each_plane_on_pipe(&data->display, pipe, plane)
			igt_plane_set_fb(plane, NULL);

		cleanup_planes(data, pipe);

		igt_assert_crc_equal(&data->ref_crc, &crc);
	}

	test_fini(data, output, pipe);
}

static void
test_plane_position(data_t *data, enum pipe pipe, uint64_t tiling)
{
	igt_output_t *output;
	int connected_outs = 0;

	if (!opt.user_seed)
		opt.seed = time(NULL);

	srand(opt.seed);

	for_each_valid_output_on_pipe(&data->display, pipe, output) {
		test_plane_position_with_output(data, pipe, output, tiling);
		connected_outs++;
	}

	igt_skip_on(connected_outs == 0);

}

static void
run_tests_for_pipe(data_t *data, enum pipe pipe)
{
	igt_output_t *output;

	igt_fixture {
		int valid_tests = 0;

		igt_skip_on(pipe >= data->display.n_pipes);

		for_each_valid_output_on_pipe(&data->display, pipe, output)
			valid_tests++;

		igt_require_f(valid_tests, "no valid crtc/connector combinations found\n");
		igt_require(data->display.pipes[pipe].n_planes > 0);
	}

	igt_subtest_f("atomic-pipe-%s-tiling-x", kmstest_pipe_name(pipe))
		test_plane_position(data, pipe, LOCAL_I915_FORMAT_MOD_X_TILED);

	igt_subtest_f("atomic-pipe-%s-tiling-y", kmstest_pipe_name(pipe))
		test_plane_position(data, pipe, LOCAL_I915_FORMAT_MOD_Y_TILED);

	igt_subtest_f("atomic-pipe-%s-tiling-yf", kmstest_pipe_name(pipe))
		test_plane_position(data, pipe, LOCAL_I915_FORMAT_MOD_Yf_TILED);

	igt_subtest_f("atomic-pipe-%s-tiling-none", kmstest_pipe_name(pipe))
		test_plane_position(data, pipe, LOCAL_DRM_FORMAT_MOD_NONE);
}

static data_t data;

static int opt_handler(int option, int option_index, void *input)
{
	switch (option) {
	case 'i':
		opt.iterations = strtol(optarg, NULL, 0);

		if (opt.iterations < LOOP_FOREVER || opt.iterations == 0) {
			igt_info("incorrect number of iterations\n");
			igt_assert(false);
		}

		break;
	case 's':
		opt.user_seed = true;
		opt.seed = strtol(optarg, NULL, 0);
		break;
	default:
		igt_assert(false);
	}

	return 0;
}

const char *help_str =
	"  --iterations Number of iterations for test coverage. -1 loop forever, default 64 iterations\n"
	"  --seed       Seed for random number generator\n";

int main(int argc, char *argv[])
{
	struct option long_options[] = {
		{ "iterations", required_argument, NULL, 'i'},
		{ "seed",    required_argument, NULL, 's'},
		{ 0, 0, 0, 0 }
	};
	enum pipe pipe;

	igt_subtest_init_parse_opts(&argc, argv, "", long_options, help_str,
				    opt_handler, NULL);

	igt_skip_on_simulation();

	igt_fixture {
		data.drm_fd = drm_open_driver_master(DRIVER_INTEL | DRIVER_AMDGPU);
		kmstest_set_vt_graphics_mode();
		igt_require_pipe_crc(data.drm_fd);
		igt_display_require(&data.display, data.drm_fd);
		igt_require(data.display.is_atomic);
	}

	for_each_pipe_static(pipe) {
		igt_subtest_group
			run_tests_for_pipe(&data, pipe);
	}

	igt_fixture {
		igt_display_fini(&data.display);
	}

	igt_exit();
}
