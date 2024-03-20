/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2023 Intel Corporation
 */

#ifndef __INTEL_MULTIGPU_H
#define __INTEL_MULTIGPU_H

#include "drmtest.h"
#include "igt_core.h"

int igt_multigpu_count_class(int chipset);
int igt_require_filtered_multigpu(int count);
int igt_require_multigpu(int count, unsigned int chipset);

#define igt_foreach_gpu(fd__, chipset__) \
	for (int igt_unique(i) = 0, fd__; \
		(fd__ = __drm_open_driver_another(igt_unique(i)++, (chipset__))) >= 0; \
		__drm_close_driver(fd__))

#define igt_multi_fork_foreach_gpu_num(__fd, __gpu_idx, __chipset, __wanted) \
	for (int igt_unique(__j) = igt_require_multigpu((__wanted), (__chipset)); \
	     igt_unique(__j) != -1; \
	     igt_unique(__j) = -1) \
		igt_multi_fork(__gpu_idx, igt_unique(__j)) \
			for (int __fd = drm_open_driver_another(__gpu_idx, (__chipset)); \
			     __fd >= 0; \
			     drm_close_driver(__fd), __fd = -1)

#define igt_multi_fork_foreach_gpu(__fd, __gpu_idx, __chipset) \
		igt_multi_fork_foreach_gpu_num(__fd, __gpu_idx, (__chipset), 1)

#define igt_multi_fork_foreach_multigpu(__fd, __gpu_idx, __chipset) \
		igt_multi_fork_foreach_gpu_num(__fd, __gpu_idx, (__chipset), 2)

#endif /* __INTEL_MULTIGPU_H */
