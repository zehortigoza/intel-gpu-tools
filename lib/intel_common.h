/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2024 Intel Corporation
 */

#ifndef __INTEL_COMMON_H__
#define __INTEL_COMMON_H__

/**
 * SECTION:intel_common
 * @short_description: i915/xe common library code
 * @title: Intel library
 * @include: intel_common.h
 *
 */

#include <stdbool.h>
#include <stdint.h>

bool is_intel_dgfx(int fd);
bool is_intel_system_region(int fd, uint64_t region);
bool is_intel_vram_region(int fd, uint64_t region);
bool is_intel_region_compressible(int fd, uint64_t region);

#endif
