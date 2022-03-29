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
 */

#include "drmtest.h"
#include "igt_drrs.h"

#define DRRS_TYPE_STR "Type: "
#define DRRS_ENABLE_STR "Enabled: "
#define DRRS_ACTIVE_STR "Active: "
#define DRRS_REFRESH_RATE_STR "Refresh rate: "
#define DRRS_STATUS_MAX_LEN 1024

static bool parse(int debugfs_fd, const char *name, const char *positive_value)
{
	char buf[DRRS_STATUS_MAX_LEN], *ch;
	int ret;

	ret = igt_debugfs_simple_read(debugfs_fd, "i915_drrs_status", buf,
				      sizeof(buf));
	if (ret < 0) {
		igt_info("Could not read i915_drrs_status: %s\n",
			 strerror(-ret));
		return false;
	}

	ch = strstr(buf, name);
	if (!ch)
		return false;
	ch += strlen(name);

	return strncmp(ch, positive_value, strlen(positive_value)) == 0;
}

bool drrs_is_seamless_supported(int debugfs_fd)
{
	return parse(debugfs_fd, DRRS_TYPE_STR, "seamless");
}

bool drrs_is_enabled(int debugfs_fd)
{
	return parse(debugfs_fd, DRRS_ENABLE_STR, "yes");
}

bool drrs_is_active(int debugfs_fd)
{
	return parse(debugfs_fd, DRRS_ACTIVE_STR, "yes");
}

bool drrs_is_low_refresh_rate(int debugfs_fd)
{
	return parse(debugfs_fd, DRRS_REFRESH_RATE_STR, "low");
}

void drrs_print_debugfs(int debugfs_fd)
{
	char buf[DRRS_STATUS_MAX_LEN];
	int ret;

	ret = igt_debugfs_simple_read(debugfs_fd, "i915_drrs_status", buf,
				      sizeof(buf));
	if (ret < 0) {
		igt_info("Could not read i915_drrs_status: %s\n",
			 strerror(-ret));
		return;
	}

	igt_info("%s", buf);
}