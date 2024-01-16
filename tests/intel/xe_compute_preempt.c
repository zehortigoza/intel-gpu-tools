// SPDX-License-Identifier: MIT
/*
 * Copyright © 2022 Intel Corporation
 */

/**
 * TEST: Check compute-related preemption functionality
 * Category: Hardware building block
 * Sub-category: compute
 * Test category: functionality test
 */

#include <string.h>

#include "igt.h"
#include "intel_compute.h"
#include "xe/xe_query.h"

/**
 * SUBTEST: compute-preempt
 * GPU requirement: LNL
 * Description:
 *      Exercise compute walker mid thread preemption scenario
 * Functionality: compute openCL kernel
 */
static void
test_compute_preempt(int fd)
{
	igt_require_f(run_intel_compute_kernel_preempt(fd), "GPU not supported\n");
}

igt_main
{
	int xe;

	igt_fixture
		xe = drm_open_driver(DRIVER_XE);

	igt_subtest("compute-preempt")
		test_compute_preempt(xe);

	igt_fixture
		drm_close_driver(xe);

}
