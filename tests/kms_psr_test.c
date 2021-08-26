#include "igt.h"
#include <sys/timerfd.h>

#define NUM_OF_FBS 60

typedef struct {
	int drm_fd;
	igt_display_t display;
	drmModeModeInfo *mode;
	igt_output_t *output;
	struct igt_fb fbs[NUM_OF_FBS];
	uint8_t fb_in_used;
	int timerfd;
	igt_plane_t *primary;
	int param_flips_per_sec;
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

		break;
	}

	if (data->param_flips_per_sec == 0)
		data->param_flips_per_sec = data->mode->vrefresh;
}

static void setup_test(data_t *data)
{
	struct itimerspec interval;
	int r, i;

	for (i = 0; i < ARRAY_SIZE(data->fbs); i++) {
		cairo_t *cr;
		int w = (data->mode->hdisplay / ARRAY_SIZE(data->fbs)) * i;

		igt_create_color_fb(data->drm_fd,
				    data->mode->hdisplay, data->mode->vdisplay,
				    DRM_FORMAT_XRGB8888,
				    I915_FORMAT_MOD_X_TILED,
				    0.0, 0.0, 0.0,
				    &data->fbs[i]);

		cr = igt_get_cairo_ctx(data->drm_fd, &data->fbs[i]);

		cairo_set_font_size(cr, 100);
		cairo_move_to(cr, 50, 150);
		igt_cairo_printf_line(cr, align_left, 0, "Framebuffer %i", i);

		igt_paint_color(cr, 0, 300, w, 200, 0.0, 1.0, 0.0);

		cairo_move_to(cr, 50, 600);
		igt_cairo_printf_line(cr, align_left, 0, "Flips per second: %i",
				      data->param_flips_per_sec);

		igt_put_cairo_ctx(cr);
	}

	data->primary = igt_output_get_plane_type(data->output,
						  DRM_PLANE_TYPE_PRIMARY);
	igt_plane_set_fb(data->primary, &data->fbs[0]);
	igt_display_commit2(&data->display, COMMIT_ATOMIC);

	data->timerfd = timerfd_create(CLOCK_MONOTONIC, 0);
	igt_require(data->timerfd != -1);

	if (data->param_flips_per_sec == 1) {
		interval.it_value.tv_nsec = 0;
		interval.it_value.tv_sec = 1;
	} else {
		interval.it_value.tv_nsec = NSEC_PER_SEC / data->param_flips_per_sec;
		interval.it_value.tv_sec = 0;
	}
	interval.it_interval.tv_nsec = interval.it_value.tv_nsec;
	interval.it_interval.tv_sec = interval.it_value.tv_sec;
	r = timerfd_settime(data->timerfd, 0, &interval, NULL);
	igt_require_f(r != -1, "Error setting timerfd\n");
}

static void run(data_t *data)
{
	while (1) {
		struct igt_fb *fb;
		uint64_t exp;
		size_t r;

		r = read(data->timerfd, &exp, sizeof(exp));
		if (r != sizeof(uint64_t))
			break;
		if (exp == 0)
			continue;

		data->fb_in_used++;
		if (data->fb_in_used == ARRAY_SIZE(data->fbs))
			data->fb_in_used = 0;
		fb = &data->fbs[data->fb_in_used];

		igt_plane_set_fb(data->primary, fb);
		igt_display_commit2(&data->display, COMMIT_ATOMIC);
	}
}

static void teardown(data_t *data)
{
	int i;

	igt_plane_set_fb(data->primary, NULL);
	igt_display_commit2(&data->display, COMMIT_ATOMIC);

	for (i = 0; i < ARRAY_SIZE(data->fbs); i++)
		igt_remove_fb(data->drm_fd, &data->fbs[i]);
}

static int opt_handler(int opt, int opt_index, void *user_data)
{
	data_t *data = user_data;

	switch (opt) {
	case 'f':
		data->param_flips_per_sec = atoi(optarg);
		igt_info("param_flips_per_sec=%i\n", data->param_flips_per_sec);
		break;
	default:
		return IGT_OPT_HANDLER_ERROR;
	}

	return IGT_OPT_HANDLER_SUCCESS;
}

const char *help_str =
	"  --flips-per-second <num>";
static struct option long_options[] = {
	{"flips-per-second", required_argument, 0, 'f'},
	{ 0, 0, 0, 0 }
};

static data_t _data;

igt_main_args("", long_options, help_str, opt_handler, &_data)
{
	data_t *data = &_data;

	igt_fixture {
		data->drm_fd = drm_open_driver_master(DRIVER_INTEL);
		kmstest_set_vt_graphics_mode();

		igt_display_require(&data->display, data->drm_fd);
		setup_output(data);
	}

	igt_subtest("main") {
		setup_test(data);
		run(data);
		teardown(data);
	}

	igt_fixture {
		igt_display_fini(&data->display);
	}
}