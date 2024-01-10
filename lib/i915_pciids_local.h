/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2022 Intel Corporation
 */
#ifndef _I915_PCIIDS_LOCAL_H_
#define _I915_PCIIDS_LOCAL_H_

#include "i915_pciids.h"

/* MTL perf */
#ifndef INTEL_MTL_M_IDS
#define INTEL_MTL_M_IDS(info) \
	INTEL_VGA_DEVICE(0x7D60, info)
#endif

#ifndef INTEL_MTL_P_GT2_IDS
#define INTEL_MTL_P_GT2_IDS(info) \
	INTEL_VGA_DEVICE(0x7D45, info)
#endif

#ifndef INTEL_MTL_P_GT3_IDS
#define INTEL_MTL_P_GT3_IDS(info) \
	INTEL_VGA_DEVICE(0x7D55, info), \
	INTEL_VGA_DEVICE(0x7DD5, info)
#endif

#ifndef INTEL_MTL_P_IDS
#define INTEL_MTL_P_IDS(info) \
	INTEL_MTL_P_GT2_IDS(info), \
	INTEL_MTL_P_GT3_IDS(info)
#endif

/* PVC */
#ifndef INTEL_PVC_IDS
#define INTEL_PVC_IDS(info) \
	INTEL_VGA_DEVICE(0x0BD0, info), \
	INTEL_VGA_DEVICE(0x0BD1, info), \
	INTEL_VGA_DEVICE(0x0BD2, info), \
	INTEL_VGA_DEVICE(0x0BD5, info), \
	INTEL_VGA_DEVICE(0x0BD6, info), \
	INTEL_VGA_DEVICE(0x0BD7, info), \
	INTEL_VGA_DEVICE(0x0BD8, info), \
	INTEL_VGA_DEVICE(0x0BD9, info), \
	INTEL_VGA_DEVICE(0x0BDA, info), \
	INTEL_VGA_DEVICE(0x0BDB, info)
#endif

#endif /* _I915_PCIIDS_LOCAL_H */
