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

#define DRRS_ENABLE_STR "DRRS Enabled: "
#define DRRS_ACTIVE_STR "DRRS Active: "
#define DRRS_REFRESH_RATE_STR "DRRS refresh rate: "

struct drrs_status {
	bool enabled;
	bool active;
	bool low_refresh_rate;
};

static bool is_yes_or_no(char *ch)
{
	return strncmp(ch, "yes", 3) == 0;
}

static const char *yes_or_no(bool r)
{
	return r ? "yes" : "no";
}

static bool parse(int debugfs_fd, enum pipe pipe, struct drrs_status *status)
{
	char buf[1024], search[16], *ch;
	int ret;

	ret = igt_debugfs_simple_read(debugfs_fd, "i915_drrs_status", buf,
				      sizeof(buf));
	if (ret < 0) {
		igt_info("Could not read i915_drrs_status: %s\n",
			 strerror(-ret));
		return false;
	}

	snprintf(search, sizeof(search), ":pipe %s]:", kmstest_pipe_name(pipe));
	ch = strstr(buf, search);
	if (!ch)
		return false;

	ch = strstr(buf, DRRS_ENABLE_STR);
	if (!ch)
		return false;
	ch += sizeof(DRRS_ENABLE_STR);
	status->enabled = is_yes_or_no(ch);

	ch = strstr(buf, DRRS_ACTIVE_STR);
	if (!ch)
		return false;
	ch += sizeof(DRRS_ACTIVE_STR);
	status->active = is_yes_or_no(ch);

	ch = strstr(buf, DRRS_REFRESH_RATE_STR);
	if (!ch)
		return false;
	ch += sizeof(DRRS_REFRESH_RATE_STR);
	status->low_refresh_rate = strncmp(ch, "low", 3) == 0;

	return true;
}

bool drrs_is_enabled(int debugfs_fd, enum pipe pipe)
{
	struct drrs_status status;
	bool ret;

	ret = parse(debugfs_fd, pipe, &status);
	if (!ret)
		return false;

	return status.enabled;
}

bool drrs_is_active(int debugfs_fd, enum pipe pipe)
{
	struct drrs_status status;
	bool ret;

	ret = parse(debugfs_fd, pipe, &status);
	if (!ret)
		return false;

	return status.active;
}

bool drrs_is_low_refresh_rate(int debugfs_fd, enum pipe pipe)
{
	struct drrs_status status;
	bool ret;

	ret = parse(debugfs_fd, pipe, &status);
	if (!ret)
		return false;

	return status.low_refresh_rate;
}

bool drrs_write_status(int debugfs_fd, enum pipe pipe, char *buf, int len)
{
	struct drrs_status status;
	int ret, used = 0;

	ret = parse(debugfs_fd, pipe, &status);
	if (!ret)
		return false;

	ret = snprintf(buf, len - used, DRRS_ENABLE_STR "%s\n",
		       yes_or_no(status.enabled));
	if (ret < 0 || ret >= (len - used))
		return false;
	used += ret;
	buf += ret;

	ret = snprintf(buf, len - used, DRRS_ACTIVE_STR "%s\n",
		       yes_or_no(status.active));
	if (ret < 0 || ret >= (len - used))
		return false;
	used += ret;
	buf += ret;

	ret = snprintf(buf, len - used, DRRS_REFRESH_RATE_STR "%s\n",
		       status.low_refresh_rate ? "low" : "high");
	if (ret < 0 || ret >= (len - used))
		return false;

	return true;
}