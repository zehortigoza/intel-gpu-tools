#include "igt.h"
#include "igt_sysfs.h"

#define SQUARE_SIZE 100
#define MAX_MST_OUTPUTS 4

typedef struct {
	int drm_fd;
	igt_display_t display;
	drm_intel_bufmgr *bufmgr;
	igt_output_t *mst_output[MAX_MST_OUTPUTS];
	struct igt_fb fb[MAX_MST_OUTPUTS];
	uint8_t mst_count, prepared;
} data_t;

static void search_mst_outputs(data_t *data)
{
	igt_display_t *display = &data->display;
	igt_output_t *output;

	for_each_connected_output(display, output) {
		drmModeConnectorPtr c = output->config.connector;
		char *path;

		igt_debug("Conn %s\n", output->name);

		/* turn off */
		igt_output_set_pipe(output, PIPE_NONE);

		if (!output->props[IGT_CONNECTOR_PATH]) {
			igt_debug("\tno patch prop\n");
			continue;
		}

		igt_debug("Found MST connector\n");

		/*if (!c->prop_values[IGT_CONNECTOR_PATH])
			continue;*/

		if (data->mst_count == MAX_MST_OUTPUTS)
			continue;

		data->mst_output[data->mst_count] = output;
		data->mst_count++;
		/*path = (char *)c->prop_values[IGT_CONNECTOR_PATH];
		igt_debug("Found MST connector with path %s\n", path);*/
	}

	igt_require_f(data->mst_count, "No MST connector found\n");
	igt_display_commit2(&data->display, COMMIT_ATOMIC);
}

static void prepare(data_t *data, uint8_t max_outputs)
{
	uint8_t i;
	igt_output_t *output;
	const drmModeModeInfo *mode = igt_std_1024_mode_get();

	for (i = 0; i < data->mst_count && i < max_outputs; i++) {
		igt_plane_t *primary;
		cairo_t *cr;
		int x, y;

		output = data->mst_output[i];
		igt_output_override_mode(output, mode);
		igt_output_set_pipe(output, PIPE_A + i);

		primary = igt_output_get_plane_type(output,
						    DRM_PLANE_TYPE_PRIMARY);
		igt_create_color_fb(data->drm_fd, mode->hdisplay, mode->vdisplay,
				    DRM_FORMAT_XRGB8888, LOCAL_DRM_FORMAT_MOD_NONE,
				    1.0, 1.0, 1.0, &data->fb[i]);

		cr = igt_get_cairo_ctx(data->drm_fd, &data->fb[i]);

		/* paint a green square in 4 different corners */
		switch (i % 4) {
		case 0:
			x = y = 0;
			break;
		case 1:
			x = mode->hdisplay - SQUARE_SIZE;
			y = 0;
			break;
		case 2:
			x = 0;
			y = mode->vdisplay - SQUARE_SIZE;
			break;
		default:
		case 4:
			x = mode->hdisplay - SQUARE_SIZE;
			y = mode->vdisplay - SQUARE_SIZE;
			break;
		}
		igt_paint_color_alpha(cr, x, y, SQUARE_SIZE, SQUARE_SIZE,
				      0.0, 1.0, 0.0, 1.0);

		igt_plane_set_fb(primary, &data->fb[i]);
		igt_put_cairo_ctx(data->drm_fd,  &data->fb[i], cr);
	}

	igt_display_commit2(&data->display, COMMIT_ATOMIC);
	data->prepared = max_outputs;
}

static void cleanup(data_t *data)
{
	igt_output_t *output;
	uint8_t i;

	for (i = 0; i < data->prepared; i++) {
		igt_plane_t *primary;

		output = data->mst_output[i];

		if (output->pending_pipe == PIPE_NONE)
			continue;

		primary = igt_output_get_plane_type(output,
						    DRM_PLANE_TYPE_PRIMARY);
		igt_plane_set_fb(primary, NULL);
		igt_output_set_pipe(output, PIPE_NONE);
	}

	igt_display_commit2(&data->display, COMMIT_ATOMIC);

	for (i = 0; i < data->prepared; i++)
		igt_remove_fb(data->drm_fd, &data->fb[i]);
}

int main(int argc, char *argv[])
{
	data_t data = {};

	igt_subtest_init_parse_opts(&argc, argv, "", NULL,
				    NULL, NULL, NULL);
	igt_skip_on_simulation();

	igt_fixture {
		data.drm_fd = drm_open_driver_master(DRIVER_INTEL);
		kmstest_set_vt_graphics_mode();

		igt_display_require(&data.display, data.drm_fd);
		search_mst_outputs(&data);

		data.bufmgr = drm_intel_bufmgr_gem_init(data.drm_fd, 4096);
		igt_assert(data.bufmgr);
		drm_intel_bufmgr_gem_enable_reuse(data.bufmgr);
	}

	igt_subtest_f("single") {
		prepare(&data, 1);
		igt_debug_manual_check("all", "single output");
		cleanup(&data);
	}

	igt_subtest_f("dual") {
		igt_require(data.mst_count >= 2);
		prepare(&data, 2);
		igt_debug_manual_check("all", "dual output");
		cleanup(&data);
	}

	igt_subtest_f("dual_single") {
		igt_require(data.mst_count >= 2);

		prepare(&data, 2);
		igt_debug_manual_check("all", "dual output");

		igt_output_set_pipe(data.mst_output[0], PIPE_NONE);
		igt_display_commit2(&data.display, COMMIT_ATOMIC);
		igt_debug_manual_check("all", "first mst disabled");
		cleanup(&data);

		prepare(&data, 2);
		igt_debug_manual_check("all", "dual output");

		igt_output_set_pipe(data.mst_output[1], PIPE_NONE);
		igt_display_commit2(&data.display, COMMIT_ATOMIC);
		igt_debug_manual_check("all", "second mst disabled");
		cleanup(&data);
	}

	igt_fixture {
		igt_display_reset(&data.display);
		drm_intel_bufmgr_destroy(data.bufmgr);
		igt_display_fini(&data.display);
	}

	igt_exit();
}
