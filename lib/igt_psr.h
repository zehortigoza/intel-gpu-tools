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
 */

#ifndef IGT_PSR_H
#define IGT_PSR_H

#include "igt_debugfs.h"
#include "igt_core.h"
#include "igt_aux.h"

#define PSR_STATUS_MAX_LEN 512

bool psr_wait_entry(int debugfs_fd);
bool psr_wait_exit(int debugfs_fd);
bool psr_enable(int debugfs_fd);
bool psr_disable(int debugfs_fd);
bool psr_sink_support(int debugfs_fd);

bool psr2_wait_deep_sleep(int debugfs_fd);
bool psr2_wait_update(int debugfs_fd);
bool psr2_enable(int debugfs_fd);
bool psr2_disable(int debugfs_fd);
bool psr2_sink_support(int debugfs_fd);

#endif
