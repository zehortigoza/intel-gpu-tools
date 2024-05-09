/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2023 Intel Corporation
 *
 * Authors:
 *    Matthew Brost <matthew.brost@intel.com>
 */

#ifndef XE_QUERY_H
#define XE_QUERY_H

#include <stdint.h>
#include <xe_drm.h>

#include "igt_aux.h"
#include "igt_list.h"
#include "igt_sizes.h"

#define XE_DEFAULT_ALIGNMENT           SZ_4K
#define XE_DEFAULT_ALIGNMENT_64K       SZ_64K

struct xe_device {
	/** @fd: xe fd */
	int fd;

	/** @config: xe configuration */
	struct drm_xe_query_config *config;

	/** @gt_list: gt info */
	struct drm_xe_query_gt_list *gt_list;

	/** @gt_list: bitmask of all memory regions */
	uint64_t memory_regions;

	/** @engines: hardware engines */
	struct drm_xe_query_engines *engines;

	/** @mem_regions: regions memory information and usage */
	struct drm_xe_query_mem_regions *mem_regions;

	/** @oa_units: information about OA units */
	struct drm_xe_query_oa_units *oa_units;

	/** @vram_size: array of vram sizes for all gt_list */
	uint64_t *vram_size;

	/** @visible_vram_size: array of visible vram sizes for all gt_list */
	uint64_t *visible_vram_size;

	/** @default_alignment: safe alignment regardless region location */
	uint32_t default_alignment;

	/** @has_vram: true if gpu has vram, false if system memory only */
	bool has_vram;

	/** @va_bits: va length in bits */
	uint32_t va_bits;

	/** @dev_id: Device id of xe device */
	uint16_t dev_id;
};

#define xe_for_each_engine(__fd, __hwe) \
	for (int igt_unique(__i) = 0; igt_unique(__i) < xe_number_engines(__fd) && \
	     (__hwe = &xe_engine(__fd, igt_unique(__i))->instance); ++igt_unique(__i))
#define xe_for_each_engine_class(__class) \
	for (__class = 0; __class < DRM_XE_ENGINE_CLASS_COMPUTE + 1; \
	     ++__class)
#define xe_for_each_gt(__fd, __gt) \
	for (__gt = 0; __gt < xe_number_gt(__fd); ++__gt)

#define xe_for_each_mem_region(__fd, __memreg, __r) \
	for (uint64_t igt_unique(__i) = 0; igt_unique(__i) < igt_fls(__memreg); igt_unique(__i)++) \
		for_if(__r = (__memreg & (1ull << igt_unique(__i))))

#define XE_IS_CLASS_SYSMEM(__region) ((__region)->mem_class == DRM_XE_MEM_REGION_CLASS_SYSMEM)
#define XE_IS_CLASS_VRAM(__region) ((__region)->mem_class == DRM_XE_MEM_REGION_CLASS_VRAM)

unsigned int xe_number_gt(int fd);
uint64_t all_memory_regions(int fd);
uint64_t system_memory(int fd);
uint64_t vram_memory(int fd, int gt);
uint64_t vram_if_possible(int fd, int gt);
struct drm_xe_engine *xe_engines(int fd);
struct drm_xe_engine *xe_engine(int fd, int idx);
struct drm_xe_mem_region *xe_mem_region(int fd, uint64_t region);
const char *xe_region_name(uint64_t region);
uint16_t xe_region_class(int fd, uint64_t region);
uint32_t xe_min_page_size(int fd, uint64_t region);
struct drm_xe_query_config *xe_config(int fd);
struct drm_xe_query_gt_list *xe_gt_list(int fd);
struct drm_xe_query_oa_units *xe_oa_units(int fd);
unsigned int xe_number_engines(int fd);
bool xe_has_vram(int fd);
uint64_t xe_vram_size(int fd, int gt);
uint64_t xe_visible_vram_size(int fd, int gt);
uint64_t xe_available_vram_size(int fd, int gt);
uint64_t xe_visible_available_vram_size(int fd, int gt);
uint32_t xe_get_default_alignment(int fd);
uint32_t xe_va_bits(int fd);
uint16_t xe_dev_id(int fd);
bool xe_supports_faults(int fd);
const char *xe_engine_class_string(uint32_t engine_class);
bool xe_has_engine_class(int fd, uint16_t engine_class);
bool xe_has_media_gt(int fd);
bool xe_is_media_gt(int fd, int gt);
uint16_t xe_gt_get_tile_id(int fd, int gt);

struct xe_device *xe_device_get(int fd);
void xe_device_put(int fd);

#define MS_TO_NS(ms) (((int64_t)ms) * 1000000)

#endif	/* XE_QUERY_H */
