/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2023 Intel Corporation
 */

#ifndef XE_OA_DATA_H
#define XE_OA_DATA_H

#ifdef __cplusplus
extern "C" {
#endif

#include <i915_drm.h>

/* The structures below are embedded in the i915-perf stream so as to
 * provide metadata. The types used in the
 * drm_i915_perf_record_header.type are defined in
 * intel_xe_perf_record_type.
 */

#include <stdint.h>

enum intel_xe_perf_record_type {
	/* Start at 65536, which is pretty safe since after 3years the
	 * kernel hasn't defined more than 3 entries.
	 */

	INTEL_XE_PERF_RECORD_TYPE_VERSION = 1 << 16,

	/* intel_xe_perf_record_device_info */
	INTEL_XE_PERF_RECORD_TYPE_DEVICE_INFO,

	/* intel_xe_perf_record_device_topology */
	INTEL_XE_PERF_RECORD_TYPE_DEVICE_TOPOLOGY,

	/* intel_xe_perf_record_timestamp_correlation */
	INTEL_XE_PERF_RECORD_TYPE_TIMESTAMP_CORRELATION,
};

/* This structure cannot ever change. */
struct intel_xe_perf_record_version {
	/* Version of the i915-perf file recording format (effectively
	 * versioning this file).
	 */
	uint32_t version;

#define INTEL_XE_PERF_RECORD_VERSION (1)

	uint32_t pad;
} __attribute__((packed));

struct intel_xe_perf_record_device_info {
	/* Frequency of the timestamps in the records. */
	uint64_t timestamp_frequency;

	/* PCI ID */
	uint32_t device_id;

	/* Stepping */
	uint32_t device_revision;

	/* GT min/max frequencies */
	uint32_t gt_min_frequency;
	uint32_t gt_max_frequency;

	/* Engine */
	uint32_t engine_class;
	uint32_t engine_instance;

	/* enum drm_i915_oa_format */
	uint32_t oa_format;

	/* Metric set name */
	char metric_set_name[256];

	/* Configuration identifier */
	char metric_set_uuid[40];

	uint32_t pad;
 } __attribute__((packed));

/* Topology as reported by i915 (variable length, aligned by the
 * recorder). */
struct intel_xe_perf_record_device_topology {
	struct drm_i915_query_topology_info topology;
};

/* Timestamp correlation between CPU/GPU. */
struct intel_xe_perf_record_timestamp_correlation {
	/* In CLOCK_MONOTONIC */
	uint64_t cpu_timestamp;

	/* Engine timestamp associated with the OA unit */
	uint64_t gpu_timestamp;
} __attribute__((packed));

#ifdef __cplusplus
};
#endif

#endif /* XE_OA_DATA_H */
