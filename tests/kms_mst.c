#include <ctype.h>
#include <string.h>

#include "igt.h"

IGT_TEST_DESCRIPTION("Test DP MST corner cases.");
/*
 * It is testing only the first MST port found, to test multiple ports or force
 * test in a specific port changes will be needed
 */

#define SQUARE_SIZE 100
#define MAX_MST_OUTPUTS 3
#define RESOLUTION_H 1920
#define RESOLUTION_V 1080
#define ANOTHER_RESOLUTION_H 1024
#define ANOTHER_RESOLUTION_V 768

typedef struct {
	int drm_fd;
	igt_display_t display;
	igt_output_t *mst_output[MAX_MST_OUTPUTS];
	struct igt_fb fb[MAX_MST_OUTPUTS];
	uint8_t mst_connectors, prepared;
} data_t;

static drmModeModeInfo *find_mode(igt_output_t *output, uint16_t h, uint16_t v,
				  uint32_t *mode_clock)
{
	drmModeModeInfo *ret = NULL;
	int i;

	for (i = 0; i < output->config.connector->count_modes; i++) {
		drmModeModeInfo *mode;

		mode = &output->config.connector->modes[i];
		if (mode->hdisplay != h ||
		    mode->vdisplay != v)
			continue;

		if (mode_clock && *mode_clock && mode->clock != *mode_clock)
			continue;

		if (mode_clock && !*mode_clock)
			*mode_clock = mode->clock;

		ret = mode;

		break;
	}

	return ret;
}

static int16_t get_parent_conn_id(const char *mst_path)
{
	int i;
	char buffer[6];

	if (mst_path[0] != 'm' || mst_path[1] != 's' || mst_path[2] != 't' ||
	    mst_path[3] != ':')
		return -1;

	/* Unlikely to need more than 5 digits to represent a connector id */
	for (i = 4; i < 9; i++) {
		if (!isdigit(mst_path[i]))
			break;
	}

	if (i == 4)
		return -1;

	i = i - 4;
	memcpy(buffer, &mst_path[4], i);
	buffer[i] = 0;
	return atoi(buffer);
}

static void search_mst_outputs(data_t *data)
{
	igt_display_t *display = &data->display;
	igt_output_t *output;
	uint32_t clock = 0;
	int16_t parent_conn = -1;

	for_each_connected_output(display, output) {
		drmModePropertyBlobPtr blob;
		char mst_path_buffer[32];
		drmModeModeInfo *mode;
		uint64_t val;

		igt_output_set_pipe(output, PIPE_NONE);

		if (data->mst_connectors == MAX_MST_OUTPUTS)
			continue;

		if (!igt_output_has_prop(output, IGT_CONNECTOR_PATH)) {
			igt_debug("\tno patch prop\n");
			continue;
		}

		val = igt_output_get_prop(output, IGT_CONNECTOR_PATH);
		blob = drmModeGetPropertyBlob(data->drm_fd, val);
		strncpy(mst_path_buffer, (const char *)blob->data,
			sizeof(mst_path_buffer) - 1);
		drmModeFreePropertyBlob(blob);

		if (parent_conn != -1) {
			if (get_parent_conn_id(mst_path_buffer) != parent_conn)
				continue;
		} else {
			parent_conn = get_parent_conn_id(mst_path_buffer);
			igt_info("MST parent connector %i\n", parent_conn);
		}

		mode = find_mode(output, RESOLUTION_H, RESOLUTION_V, &clock);
		if (!mode) {
			igt_info("MST connector %s[%s] found but no compatible mode found\n",
				 output->name, mst_path_buffer);
			continue;
		}

		igt_info("Added MST connector %s[%s]\n", output->name, mst_path_buffer);

		igt_output_override_mode(output, mode);
		data->mst_output[data->mst_connectors] = output;
		data->mst_connectors++;
	}

	igt_require_f(data->mst_connectors, "No MST connector found\n");
	igt_display_commit2(&data->display, COMMIT_ATOMIC);
}

static void prepare(data_t *data, uint8_t n_outputs)
{
	igt_output_t *output;
	uint8_t i;

	for (i = 0; i < data->mst_connectors && i < n_outputs; i++) {
		const drmModeModeInfo *mode;
		igt_plane_t *primary;
		double r, g, b;
		cairo_t *cr;

		output = data->mst_output[i];
		mode = igt_output_get_mode(output);

		igt_output_set_pipe(output, PIPE_A + i);

		primary = igt_output_get_plane_type(output,
						    DRM_PLANE_TYPE_PRIMARY);
		igt_create_color_fb(data->drm_fd, mode->hdisplay, mode->vdisplay,
				    DRM_FORMAT_XRGB8888, LOCAL_DRM_FORMAT_MOD_NONE,
				    1.0, 1.0, 1.0, &data->fb[i]);

		cr = igt_get_cairo_ctx(data->drm_fd, &data->fb[i]);

		/* paint square in different color */
		switch (i % 4) {
		case 0:
			r = 1.0;
			g = b = 0.0;
			break;
		case 1:
			g = 1.0;
			r = b = 0.0;
			break;
		case 2:
			b = 1.0;
			r = g = 0.0;
			break;
		default:
		case 4:
			r = g = b = 0.0;
			break;
		}
		igt_paint_color_alpha(cr, 0, 0, SQUARE_SIZE, SQUARE_SIZE,
				      r, g, b, 1.0);

		igt_plane_set_fb(primary, &data->fb[i]);
		igt_put_cairo_ctx(data->drm_fd,  &data->fb[i], cr);
	}

	igt_display_commit2(&data->display, COMMIT_ATOMIC);
	data->prepared = n_outputs;
}

static void change_resolution(data_t *data, bool all)
{
	igt_output_t *output;
	uint32_t clock = 0;
	uint8_t i;

	for (i = 0; i < data->mst_connectors; i++) {
		drmModeModeInfo *mode;

		output = data->mst_output[i];
		if (output->pending_pipe == PIPE_NONE)
			continue;

		mode = find_mode(output, ANOTHER_RESOLUTION_H,
				 ANOTHER_RESOLUTION_V, &clock);
		if (!mode) {
			igt_info("New resolution not found on %s skipping it\n",
				 output->name);
			igt_output_set_pipe(output, PIPE_NONE);
			continue;
		}

		igt_output_override_mode(output, mode);

		if (!all)
			break;
	}

	igt_display_commit2(&data->display, COMMIT_ATOMIC);
}

static void restore_resolution(data_t *data)
{
	igt_output_t *output;
	uint32_t clock = 0;
	uint8_t i;

	for (i = 0; i < data->mst_connectors; i++) {
		drmModeModeInfo *mode;

		output = data->mst_output[i];
		mode = find_mode(output, RESOLUTION_H, RESOLUTION_V, &clock);
		/*
		 * No need to check because this mode was already found in
		 * search_mst_outputs()
		 */
		igt_output_override_mode(output, mode);
		/* Reenable any sink left off in change_resolution() */
		igt_output_set_pipe(output, PIPE_A + i);
	}

	igt_display_commit2(&data->display, COMMIT_ATOMIC);
}

static void cleanup(data_t *data)
{
	uint8_t i;

	for (i = 0; i < data->prepared; i++) {
		igt_output_t *output = data->mst_output[i];
		igt_plane_t *primary;

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
	}

	igt_describe("Enable all MST streams in the same port");
	igt_subtest_f("all_enabled") {
		prepare(&data, data.mst_connectors);
		igt_debug_manual_check("all", "all streams enabled");
		cleanup(&data);
	}

	igt_describe("Test change the master CRTC of the MST stream");
	igt_subtest_f("change_master") {
		enum pipe pipe;

		igt_require(data.mst_connectors >= 2);

		prepare(&data, 2);
		igt_debug_manual_check("all", "dual output");

		pipe = data.mst_output[0]->pending_pipe;
		igt_output_set_pipe(data.mst_output[0], PIPE_NONE);
		igt_display_commit2(&data.display, COMMIT_ATOMIC);
		igt_debug_manual_check("all", "first MST stream disabled");

		igt_output_set_pipe(data.mst_output[0], pipe);
		igt_display_commit2(&data.display, COMMIT_ATOMIC);
		igt_debug_manual_check("all", "dual output");

		pipe = data.mst_output[1]->pending_pipe;
		igt_output_set_pipe(data.mst_output[1], PIPE_NONE);
		igt_display_commit2(&data.display, COMMIT_ATOMIC);
		igt_debug_manual_check("all", "second MST stream disabled");

		igt_output_set_pipe(data.mst_output[1], pipe);
		igt_display_commit2(&data.display, COMMIT_ATOMIC);
		igt_debug_manual_check("all", "dual output");

		cleanup(&data);
	}

	igt_describe("Test change the resolution off all MST streams");
	igt_subtest_f("change_resolution") {
		prepare(&data, data.mst_connectors);
		igt_debug_manual_check("all", "regular resolution in all streams");

		change_resolution(&data, true);
		igt_debug_manual_check("all", "new resolution set in compatible streams");

		restore_resolution(&data);
		igt_debug_manual_check("all", "regular resolution in all streams again");
		cleanup(&data);
	}

	igt_describe("Test do a fullmodeset in the master CRTC of the MST stream");
	igt_subtest_f("fullmodeset_master") {
		prepare(&data, data.mst_connectors);
		igt_debug_manual_check("all", "regular resolution in all streams");

		change_resolution(&data, false);
		igt_debug_manual_check("all", "new resolution in the first stream");

		restore_resolution(&data);
		igt_debug_manual_check("all", "regular resolution in all streams again");
		cleanup(&data);
	}

	igt_fixture {
		igt_display_reset(&data.display);
		igt_display_fini(&data.display);
	}

	igt_exit();
}
