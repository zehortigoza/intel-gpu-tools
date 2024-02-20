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
test_compute_preempt(int fd, struct drm_xe_engine_class_instance *hwe)
{
	igt_require_f(run_intel_compute_kernel_preempt(fd, hwe), "GPU not supported\n");
}

igt_main
{
	int xe;
	struct drm_xe_engine_class_instance *hwe;

	igt_fixture
		xe = drm_open_driver(DRIVER_XE);

	igt_subtest_with_dynamic("compute-preempt") {
		xe_for_each_engine(xe, hwe) {
			if (hwe->engine_class != DRM_XE_ENGINE_CLASS_COMPUTE &&
			    hwe->engine_class != DRM_XE_ENGINE_CLASS_RENDER)
				continue;

			igt_dynamic_f("engine-%s", xe_engine_class_string(hwe->engine_class))
				test_compute_preempt(xe, hwe);
		}
	}

	igt_fixture
		drm_close_driver(xe);

}
