/* SPDX-License-Identifier: MIT */
/*
 * Copyright © 2023 Intel Corporation
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <inttypes.h>
#include <errno.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/times.h>
#include <sys/types.h>
#include <dirent.h>
#include <time.h>
#include <poll.h>
#include <math.h>

#include "i915/gem.h"
#include "i915/gem_engine_topology.h"
#include "i915/perf.h"
#include "igt.h"
#include "igt_device.h"
#include "igt_perf.h"
#include "igt_sysfs.h"
#include "drm.h"
#include "xe/xe_ioctl.h"
#include "xe/xe_query.h"

/**
 * TEST: xe_oa
 * Description: Test the xe perf metrics streaming interface
 * Run type: FULL
 *
 * SUBTEST: blocking
 * Description: Test blocking read with default hrtimer frequency
 * Feature: xe streaming interface, oa
 * Test category: Perf
 *
 * SUBTEST: blocking-parameterized
 * Description: Test blocking read with different hrtimer frequencies
 * Feature: xe streaming interface, oa
 * Test category: Perf
 *
 * SUBTEST: buffer-fill
 * Description: Test the xe perf metrics streaming interface
 * Feature: xe streaming interface, oa
 * Test category: Perf
 *
 * SUBTEST: create-destroy-userspace-config
 * Description: Test the xe perf metrics streaming interface
 * Feature: xe streaming interface, oa
 * Test category: Perf
 *
 * SUBTEST: disabled-read-error
 * Description: Test the xe perf metrics streaming interface
 * Feature: xe streaming interface, oa
 * Test category: Perf
 *
 * SUBTEST: enable-disable
 * Feature: oa
 *
 * SUBTEST: gen12-group-concurrent-oa-buffer-read
 * Description: Verify concurrent reads from OA buffers in different groups
 * Feature: oa
 *
 * SUBTEST: gen12-group-exclusive-stream-ctx-handle
 * Description: Verify exclusivity of perf streams with ctx handle
 * Feature: oa
 *
 * SUBTEST: gen12-group-exclusive-stream-sample-oa
 * Description: Verify exclusivity of perf streams with sample oa option
 * Feature: oa
 *
 * SUBTEST: gen12-invalid-class-instance
 * Description: Verify invalid class instance
 * Feature: xe streaming interface, oa
 * Test category: Perf
 *
 * SUBTEST: gen12-mi-rpc
 * Description: Test MI REPORT PERF COUNT for Gen 12
 * Feature: oa
 *
 * SUBTEST: gen12-oa-tlb-invalidate
 * Description: Test OA TLB invalidate
 * Feature: xe streaming interface, oa
 * Test category: Perf
 *
 * SUBTEST: gen12-unprivileged-single-ctx-counters
 * Description: Measure performance for a specific context using OAR in Gen 12
 * Feature: oa
 *
 * SUBTEST: gen12-unprivileged-one-ctx
 * Description: Measure performance for a specific context using OAR in Gen 12
 * Feature: oa
 *
 * SUBTEST: gen8-unprivileged-single-ctx-counters
 * Feature: oa, obsolete
 *
 * SUBTEST: xe-ref-count
 * Description: Test the xe perf metrics streaming interface
 * Feature: xe streaming interface, oa
 * Test category: Perf
 *
 * SUBTEST: invalid-create-userspace-config
 * Description: Test the xe perf metrics streaming interface
 * Feature: xe streaming interface, oa
 * Test category: Perf
 *
 * SUBTEST: invalid-oa-exponent
 * Description: Test the xe perf metrics streaming interface
 * Feature: xe streaming interface, oa
 * Test category: Perf
 *
 * SUBTEST: invalid-oa-format-id
 * Description: Test the xe perf metrics streaming interface
 * Feature: xe streaming interface, oa
 * Test category: Perf
 *
 * SUBTEST: invalid-oa-metric-set-id
 * Description: Test the xe perf metrics streaming interface
 * Feature: xe streaming interface, oa
 * Test category: Perf
 *
 * SUBTEST: invalid-open-flags
 * Description: Test the xe perf metrics streaming interface
 * Feature: xe streaming interface, oa
 * Test category: Perf
 *
 * SUBTEST: invalid-remove-userspace-config
 * Description: Test the xe perf metrics streaming interface
 * Feature: xe streaming interface, oa
 * Test category: Perf
 *
 * SUBTEST: low-oa-exponent-permissions
 * Description: Test the xe perf metrics streaming interface
 * Feature: xe streaming interface, oa
 * Test category: Perf
 *
 * SUBTEST: mi-rpc
 * Feature: oa
 *
 * SUBTEST: missing-sample-flags
 * Description: Test the xe perf metrics streaming interface
 * Feature: xe streaming interface, oa
 * Test category: Perf
 *
 * SUBTEST: non-sampling-read-error
 * Description: Test the xe perf metrics streaming interface
 * Feature: xe streaming interface, oa
 * Test category: Perf
 *
 * SUBTEST: non-system-wide-paranoid
 * Feature: oa
 *
 * SUBTEST: non-zero-reason
 * Description: Test that reason field in OA reports is never 0 on Gen8+
 * Feature: xe streaming interface, oa
 * Test category: Perf
 *
 * SUBTEST: oa-exponents
 * Feature: oa
 *
 * SUBTEST: oa-formats
 * Description: Test the xe perf metrics streaming interface
 * Feature: xe streaming interface, oa
 * Test category: Perf
 *
 * SUBTEST: per-context-mode-unprivileged
 * Feature: oa, obsolete
 *
 * SUBTEST: polling
 * Description: Test polled read with default hrtimer frequency
 * Feature: xe streaming interface, oa
 * Test category: Perf
 *
 * SUBTEST: polling-parameterized
 * Description: Test polled read with different hrtimer frequencies
 * Feature: xe streaming interface, oa
 * Test category: Perf
 *
 * SUBTEST: polling-small-buf
 * Description: Test polled read with buffer size smaller than available data
 * Feature: xe streaming interface, oa
 * Test category: Perf
 *
 * SUBTEST: rc6-disable
 * Feature: oa
 *
 * SUBTEST: short-reads
 * Description: Test the xe perf metrics streaming interface
 * Feature: xe streaming interface, oa
 * Test category: Perf
 *
 * SUBTEST: stress-open-close
 * Description: Stress tests opening & closing the xe-perf stream in a busy loop
 * Feature: oa
 *
 * SUBTEST: sysctl-defaults
 * Description: Test the xe perf metrics streaming interface
 * Feature: xe streaming interface, oa
 * Test category: Perf
 *
 * SUBTEST: unprivileged-single-ctx-counters
 * Feature: oa, obsolete
 *
 * SUBTEST: whitelisted-registers-userspace-config
 * Description: Test the xe perf metrics streaming interface
 * Feature: xe streaming interface, oa
 * Test category: Perf
 *
 * SUBTEST: map-oa-buffer
 * Description: Test oa buffer mmap
 * Feature: oa
 *
 * SUBTEST: invalid-map-oa-buffer
 * Description: Test oa buffer mmap
 * Feature: oa
 *
 * SUBTEST: non-privileged-map-oa-buffer
 * Description: Test oa buffer mmap
 * Feature: oa
 *
 * SUBTEST: non-privileged-access-vaddr
 * Description: Test oa buffer mmap
 * Feature: oa
 *
 * SUBTEST: privileged-forked-access-vaddr
 * Description: Test oa buffer mmap
 * Feature: oa
 *
 * SUBTEST: closed-fd-and-unmapped-access
 * Description: Test oa buffer mmap
 * Feature: oa
 */

IGT_TEST_DESCRIPTION("Test the xe perf metrics streaming interface");

#define xe_relax_checks(drm_fd) (IS_LUNARLAKE(xe_dev_id(drm_fd)))

#define GEN6_MI_REPORT_PERF_COUNT ((0x28 << 23) | (3 - 2))
#define GEN8_MI_REPORT_PERF_COUNT ((0x28 << 23) | (4 - 2))

#define OAREPORT_REASON_MASK           0x3f
#define OAREPORT_REASON_SHIFT          19
#define OAREPORT_REASON_TIMER          (1<<0)
#define OAREPORT_REASON_INTERNAL       (3<<1)
#define OAREPORT_REASON_CTX_SWITCH     (1<<3)
#define OAREPORT_REASON_GO             (1<<4)
#define OAREPORT_REASON_CLK_RATIO      (1<<5)

#define PIPE_CONTROL_GLOBAL_SNAPSHOT_COUNT_RESET	(1 << 19)
#define PIPE_CONTROL_SYNC_GFDT	  (1 << 17)
#define PIPE_CONTROL_NO_WRITE	   (0 << 14)
#define PIPE_CONTROL_WRITE_IMMEDIATE    (1 << 14)
#define PIPE_CONTROL_WRITE_DEPTH_COUNT  (2 << 14)
#define PIPE_CONTROL_RENDER_TARGET_FLUSH (1 << 12)
#define PIPE_CONTROL_INSTRUCTION_INVALIDATE (1 << 11)
#define PIPE_CONTROL_ISP_DIS	    (1 << 9)
#define PIPE_CONTROL_INTERRUPT_ENABLE   (1 << 8)
/* GT */
#define PIPE_CONTROL_DATA_CACHE_INVALIDATE      (1 << 5)
#define PIPE_CONTROL_PPGTT_WRITE	(0 << 2)
#define PIPE_CONTROL_GLOBAL_GTT_WRITE   (1 << 2)

#define MAX_OA_BUF_SIZE (16 * 1024 * 1024)
#define OA_BUFFER_SIZE MAX_OA_BUF_SIZE

#define RING_FORCE_TO_NONPRIV_ADDRESS_MASK 0x03fffffc
/*
 * Engine specific registers defined as offsets from engine->mmio_base. For
 * these registers, OR bit[0] with 1 so we can add the mmio_base when running
 * engine specific test.
 */
#define MMIO_BASE_OFFSET 0x1

#define GEN12_OAG_OASTATUS (0xdafc)
#define GEN12_OAG_PERF_COUNTER_B(idx) (0xDA94 + 4 * (idx))
#define GEN12_OAG_OATAILPTR (0xdb04)
#define  GEN12_OAG_OATAILPTR_MASK 0xffffffc0
#define GEN12_OAG_OABUFFER (0xdb08)

#define ADD_PROPS(_head, _tail, _key, _value) \
	do { \
		igt_assert((_tail - _head) < (DRM_XE_OA_PROPERTY_MAX * 2)); \
		*_tail++ = DRM_XE_OA_PROPERTY_##_key; \
		*_tail++ = _value; \
	} while (0)

/*
 * If formats are added here remember to change
 * lib/xe/oa-configs/oa-metricset-codegen.py
 */
enum xe_oa_format_name {
	XE_OA_FORMAT_C4_B8 = 7,

	/* Gen8+ */
	XE_OA_FORMAT_A12,
	XE_OA_FORMAT_A12_B8_C8,
	XE_OA_FORMAT_A32u40_A4u32_B8_C8,

	/* DG2 */
	XE_OAR_FORMAT_A32u40_A4u32_B8_C8,
	XE_OA_FORMAT_A24u40_A14u32_B8_C8,

	/* DG2/MTL OAC */
	XE_OAC_FORMAT_A24u64_B8_C8,
	XE_OAC_FORMAT_A22u32_R2u32_B8_C8,

	/* MTL OAM */
	XE_OAM_FORMAT_MPEC8u64_B8_C8,
	XE_OAM_FORMAT_MPEC8u32_B8_C8,

	/* Xe2+ */
	XE_OA_FORMAT_PEC64u64,
	XE_OA_FORMAT_PEC64u64_B8_C8,
	XE_OA_FORMAT_PEC64u32,
	XE_OA_FORMAT_PEC32u64_G1,
	XE_OA_FORMAT_PEC32u32_G1,
	XE_OA_FORMAT_PEC32u64_G2,
	XE_OA_FORMAT_PEC32u32_G2,
	XE_OA_FORMAT_PEC36u64_G1_32_G2_4,
	XE_OA_FORMAT_PEC36u64_G1_4_G2_32,

	XE_OA_FORMAT_MAX,
};

struct accumulator {
#define MAX_RAW_OA_COUNTERS 62
	enum xe_oa_format_name format;

	uint64_t deltas[MAX_RAW_OA_COUNTERS];
};

/* OA unit types */
enum {
	OAG,
	OAR,
	OAM,

	MAX_OA_TYPE,
};

struct oa_format {
	const char *name;
	size_t size;
	int a40_high_off; /* bytes */
	int a40_low_off;
	int n_a40;
	int a64_off;
	int n_a64;
	int a_off;
	int n_a;
	int first_a;
	int first_a40;
	int b_off;
	int n_b;
	int c_off;
	int n_c;
	int oa_type; /* of enum xe_oa_format_name */
	bool report_hdr_64bit;
	int counter_select;
	int counter_size;
	int bc_report;
};

static struct oa_format gen12_oa_formats[XE_OA_FORMAT_MAX] = {
	[XE_OA_FORMAT_A32u40_A4u32_B8_C8] = {
		"A32u40_A4u32_B8_C8", .size = 256,
		.a40_high_off = 160, .a40_low_off = 16, .n_a40 = 32,
		.a_off = 144, .n_a = 4, .first_a = 32,
		.b_off = 192, .n_b = 8,
		.c_off = 224, .n_c = 8, .oa_type = DRM_XE_OA_FMT_TYPE_OAG,
		.counter_select = 5,
	},
};

static struct oa_format dg2_oa_formats[XE_OA_FORMAT_MAX] = {
	[XE_OAR_FORMAT_A32u40_A4u32_B8_C8] = {
		"A32u40_A4u32_B8_C8", .size = 256,
		.a40_high_off = 160, .a40_low_off = 16, .n_a40 = 32,
		.a_off = 144, .n_a = 4, .first_a = 32,
		.b_off = 192, .n_b = 8,
		.c_off = 224, .n_c = 8, .oa_type = DRM_XE_OA_FMT_TYPE_OAR,
		.counter_select = 5,
	},
	/* This format has A36 and A37 interleaved with high bytes of some A
	 * counters, so we will accumulate only subset of counters.
	 */
	[XE_OA_FORMAT_A24u40_A14u32_B8_C8] = {
		"A24u40_A14u32_B8_C8", .size = 256,
		/* u40: A4 - A23 */
		.a40_high_off = 160, .a40_low_off = 16, .n_a40 = 20, .first_a40 = 4,
		/* u32: A0 - A3 */
		.a_off = 16, .n_a = 4,
		.b_off = 192, .n_b = 8,
		.c_off = 224, .n_c = 8, .oa_type = DRM_XE_OA_FMT_TYPE_OAG,
		.counter_select = 5,
	},
	/* This format has 24 u64 counters ranging from A0 - A35. Until we come
	 * up with a better mechanism to define missing counters, we will use a
	 * subset of counters that are indexed by one-increments - A28 - A35.
	 */
	[XE_OAC_FORMAT_A24u64_B8_C8] = {
		"OAC_A24u64_B8_C8", .size = 320,
		.a64_off = 160, .n_a64 = 8,
		.b_off = 224, .n_b = 8,
		.c_off = 256, .n_c = 8, .oa_type = DRM_XE_OA_FMT_TYPE_OAC,
		.counter_select = 1, },
};

static struct oa_format mtl_oa_formats[XE_OA_FORMAT_MAX] = {
	[XE_OAR_FORMAT_A32u40_A4u32_B8_C8] = {
		"A32u40_A4u32_B8_C8", .size = 256,
		.a40_high_off = 160, .a40_low_off = 16, .n_a40 = 32,
		.a_off = 144, .n_a = 4, .first_a = 32,
		.b_off = 192, .n_b = 8,
		.c_off = 224, .n_c = 8, .oa_type = DRM_XE_OA_FMT_TYPE_OAR,
		.counter_select = 5,
	},
	/* This format has A36 and A37 interleaved with high bytes of some A
	 * counters, so we will accumulate only subset of counters.
	 */
	[XE_OA_FORMAT_A24u40_A14u32_B8_C8] = {
		"A24u40_A14u32_B8_C8", .size = 256,
		/* u40: A4 - A23 */
		.a40_high_off = 160, .a40_low_off = 16, .n_a40 = 20, .first_a40 = 4,
		/* u32: A0 - A3 */
		.a_off = 16, .n_a = 4,
		.b_off = 192, .n_b = 8,
		.c_off = 224, .n_c = 8, .oa_type = DRM_XE_OA_FMT_TYPE_OAG,
		.counter_select = 5,
	},

	/* Treat MPEC countes as A counters for now */
	[XE_OAM_FORMAT_MPEC8u64_B8_C8] = {
		"MPEC8u64_B8_C8", .size = 192,
		.a64_off = 32, .n_a64 = 8,
		.b_off = 96, .n_b = 8,
		.c_off = 128, .n_c = 8, .oa_type = DRM_XE_OA_FMT_TYPE_OAM_MPEC,
		.report_hdr_64bit = true,
		.counter_select = 1,
	},
	[XE_OAM_FORMAT_MPEC8u32_B8_C8] = {
		"MPEC8u32_B8_C8", .size = 128,
		.a_off = 32, .n_a = 8,
		.b_off = 64, .n_b = 8,
		.c_off = 96, .n_c = 8, .oa_type = DRM_XE_OA_FMT_TYPE_OAM_MPEC,
		.report_hdr_64bit = true,
		.counter_select = 2,
	},
	/* This format has 24 u64 counters ranging from A0 - A35. Until we come
	 * up with a better mechanism to define missing counters, we will use a
	 * subset of counters that are indexed by one-increments - A28 - A35.
	 */
	[XE_OAC_FORMAT_A24u64_B8_C8] = {
		"OAC_A24u64_B8_C8", .size = 320,
		.a64_off = 160, .n_a64 = 8,
		.b_off = 224, .n_b = 8,
		.c_off = 256, .n_c = 8, .oa_type = DRM_XE_OA_FMT_TYPE_OAC,
		.counter_select = 1, },
};

static struct oa_format lnl_oa_formats[XE_OA_FORMAT_MAX] = {
	[XE_OA_FORMAT_PEC64u64] = {
		"PEC64u64", .size = 576,
		.oa_type = DRM_XE_OA_FMT_TYPE_PEC,
		.report_hdr_64bit = true,
		.counter_select = 1,
		.counter_size = 1,
		.bc_report = 0 },
	[XE_OA_FORMAT_PEC64u64_B8_C8] = {
		"PEC64u64_B8_C8", .size = 640,
		.oa_type = DRM_XE_OA_FMT_TYPE_PEC,
		.report_hdr_64bit = true,
		.counter_select = 1,
		.counter_size = 1,
		.bc_report = 1 },
	[XE_OA_FORMAT_PEC64u32] = {
		"PEC64u32", .size = 320,
		.oa_type = DRM_XE_OA_FMT_TYPE_PEC,
		.report_hdr_64bit = true,
		.counter_select = 1,
		.counter_size = 0,
		.bc_report = 0 },
	[XE_OA_FORMAT_PEC32u64_G1] = {
		"PEC32u64_G1", .size = 320,
		.oa_type = DRM_XE_OA_FMT_TYPE_PEC,
		.report_hdr_64bit = true,
		.counter_select = 5,
		.counter_size = 1,
		.bc_report = 0 },
	[XE_OA_FORMAT_PEC32u32_G1] = {
		"PEC32u32_G1", .size = 192,
		.oa_type = DRM_XE_OA_FMT_TYPE_PEC,
		.report_hdr_64bit = true,
		.counter_select = 5,
		.counter_size = 0,
		.bc_report = 0 },
	[XE_OA_FORMAT_PEC32u64_G2] = {
		"PEC32u64_G2", .size = 320,
		.oa_type = DRM_XE_OA_FMT_TYPE_PEC,
		.report_hdr_64bit = true,
		.counter_select = 6,
		.counter_size = 1,
		.bc_report = 0 },
	[XE_OA_FORMAT_PEC32u32_G2] = {
		"PEC32u64_G2", .size = 192,
		.oa_type = DRM_XE_OA_FMT_TYPE_PEC,
		.report_hdr_64bit = true,
		.counter_select = 6,
		.counter_size = 0,
		.bc_report = 0 },
	[XE_OA_FORMAT_PEC36u64_G1_32_G2_4] = {
		"PEC36u64_G1_32_G2_4", .size = 320,
		.oa_type = DRM_XE_OA_FMT_TYPE_PEC,
		.report_hdr_64bit = true,
		.counter_select = 3,
		.counter_size = 1,
		.bc_report = 0 },
	[XE_OA_FORMAT_PEC36u64_G1_4_G2_32] = {
		"PEC36u64_G1_4_G2_32_G2", .size = 320,
		.oa_type = DRM_XE_OA_FMT_TYPE_PEC,
		.report_hdr_64bit = true,
		.counter_select = 4,
		.counter_size = 1,
		.bc_report = 0 },
};

/* No A counters currently reserved/undefined for gen8+ so far */
static bool gen8_undefined_a_counters[45];

static int drm_fd = -1;
static int sysfs = -1;
static int pm_fd = -1;
static int stream_fd = -1;
static uint32_t devid;
static struct intel_execution_engine2 default_e2;
static struct perf_engine_group *perf_oa_groups;
static uint32_t num_perf_oa_groups;

static uint64_t gt_max_freq_mhz = 0;
static struct intel_perf *intel_perf = NULL;
static bool *undefined_a_counters;
static uint64_t oa_exp_1_millisec;
struct intel_mmio_data mmio_data;

static igt_render_copyfunc_t render_copy = NULL;
static uint64_t (*read_report_ticks)(const uint32_t *report,
				     enum xe_oa_format_name format);
static void (*sanity_check_reports)(const uint32_t *oa_report0,
				    const uint32_t *oa_report1,
				    enum xe_oa_format_name format);


static struct intel_perf_metric_set *metric_set(const struct intel_execution_engine2 *e2);
#define default_test_set metric_set(&default_e2)

static void set_fd_flags(int fd, int flags)
{
	int old = fcntl(fd, F_GETFL, 0);

	igt_assert_lte(0, old);
	igt_assert_eq(0, fcntl(fd, F_SETFL, old | flags));
}

static void
dump_report(const uint32_t *report, uint32_t size, const char *message) {
	uint32_t i;
	igt_debug("%s\n", message);
	for (i = 0; i < size; i += 4) {
		igt_debug("%08x %08x %08x %08x\n",
				report[i],
				report[i + 1],
				report[i + 2],
				report[i + 3]);
	}
}

static struct oa_format
get_oa_format(enum xe_oa_format_name format)
{
	if (IS_DG2(devid))
		return dg2_oa_formats[format];
	else if (IS_METEORLAKE(devid))
		return mtl_oa_formats[format];
	else if (IS_LUNARLAKE(devid))
		return lnl_oa_formats[format];
	else
		return gen12_oa_formats[format];
}

static u64 oa_format_fields(u64 name)
{
#define FIELD_PREP_ULL(_mask, _val) \
	(((_val) << (__builtin_ffsll(_mask) - 1)) & (_mask))

	struct oa_format f = get_oa_format(name);

	/* 0 format name is invalid */
	if (!name)
		memset(&f, 0xff, sizeof(f));

	return FIELD_PREP_ULL(DRM_XE_OA_FORMAT_MASK_FMT_TYPE, (u64)f.oa_type) |
		FIELD_PREP_ULL(DRM_XE_OA_FORMAT_MASK_COUNTER_SEL, (u64)f.counter_select) |
		FIELD_PREP_ULL(DRM_XE_OA_FORMAT_MASK_COUNTER_SIZE, (u64)f.counter_size) |
		FIELD_PREP_ULL(DRM_XE_OA_FORMAT_MASK_BC_REPORT, (u64)f.bc_report);
}
#define __ff oa_format_fields

static char *
pretty_print_oa_period(uint64_t oa_period_ns)
{
	static char result[100];
	static const char *units[4] = { "ns", "us", "ms", "s" };
	double val = oa_period_ns;
	int iter = 0;

	while (iter < (ARRAY_SIZE(units) - 1) &&
	       val >= 1000.0f) {
		val /= 1000.0f;
		iter++;
	}

	snprintf(result, sizeof(result), "%.3f%s", val, units[iter]);
	return result;
}

static void
__perf_close(int fd)
{
	close(fd);
	stream_fd = -1;

	if (pm_fd >= 0) {
		close(pm_fd);
		pm_fd = -1;
	}
}

static int
__perf_open(int fd, struct drm_xe_oa_open_prop *param, bool prevent_pm)
{
	int ret;
	int32_t pm_value = 0;

	if (stream_fd >= 0)
		__perf_close(stream_fd);
	if (pm_fd >= 0) {
		close(pm_fd);
		pm_fd = -1;
	}

	ret = xe_perf_ioctl(fd, DRM_IOCTL_XE_PERF, DRM_XE_PERF_OP_STREAM_OPEN, param);

	igt_assert(ret >= 0);
	errno = 0;

	if (prevent_pm) {
		pm_fd = open("/dev/cpu_dma_latency", O_RDWR);
		igt_assert(pm_fd >= 0);

		igt_assert_eq(write(pm_fd, &pm_value, sizeof(pm_value)), sizeof(pm_value));
	}

	return ret;
}

static bool
has_param_class_instance(void)
{
	return true;
}

static uint64_t
read_u64_file(const char *path)
{
	FILE *f;
	uint64_t val;

	f = fopen(path, "r");
	igt_assert(f);

	igt_assert_eq(fscanf(f, "%"PRIu64, &val), 1);

	fclose(f);

	return val;
}

static void
write_u64_file(const char *path, uint64_t val)
{
	FILE *f;

	f = fopen(path, "w");
	igt_assert(f);

	igt_assert(fprintf(f, "%"PRIu64, val) > 0);

	fclose(f);
}

static bool
try_sysfs_read_u64(const char *path, uint64_t *val)
{
	return igt_sysfs_scanf(sysfs, path, "%"PRIu64, val) == 1;
}

static unsigned long
sysfs_read(enum i915_attr_id id)
{
	unsigned long value;

	if (is_xe_device(drm_fd)) {
		switch (id) {
		case RPS_RP0_FREQ_MHZ:
			igt_assert(igt_sysfs_scanf(sysfs, "device/tile0/gt0/freq0/rp0_freq", "%lu", &value) == 1);
			break;
		case RC6_RESIDENCY_MS:
			igt_assert(igt_sysfs_scanf(sysfs, "device/tile0/gt0/rc6_residency", "%lu", &value) == 1);
			break;
		default:
			igt_assert(0);
		}
	} else {
		igt_assert(igt_sysfs_rps_scanf(sysfs, id, "%lu", &value) == 1);
	}

	return value;
}

static uint64_t
gen8_read_report_ticks(const uint32_t *report, enum xe_oa_format_name format)
{

	struct oa_format fmt = get_oa_format(format);

	return fmt.report_hdr_64bit ? *(uint64_t *)&report[6] : report[3];
}

/*
 * t0 is a value sampled before t1. width is number of bits used to represent
 * t0/t1. Normally t1 is greater than t0. In cases where t1 < t0 use this
 * helper. Since the size of t1/t0 is already 64 bits, no special handling is
 * needed for width = 64.
 */
static uint64_t
elapsed_delta(uint64_t t1, uint64_t t0, uint32_t width)
{
	uint32_t max_bits = sizeof(t1) * 8;

	igt_assert(width <= max_bits);

	if (t1 < t0 && width != max_bits)
		return ((1ULL << width) - t0) + t1;

	return t1 - t0;
}

static uint64_t
oa_tick_delta(const uint32_t *report1,
	      const uint32_t *report0,
	      enum xe_oa_format_name format)
{
	return elapsed_delta(read_report_ticks(report1, format),
			     read_report_ticks(report0, format), 32);
}

static void
gen8_read_report_clock_ratios(const uint32_t *report,
			      uint32_t *slice_freq_mhz,
			      uint32_t *unslice_freq_mhz)
{
	uint32_t unslice_freq = report[0] & 0x1ff;
	uint32_t slice_freq_low = (report[0] >> 25) & 0x7f;
	uint32_t slice_freq_high = (report[0] >> 9) & 0x3;
	uint32_t slice_freq = slice_freq_low | (slice_freq_high << 7);

	*slice_freq_mhz = (slice_freq * 16666) / 1000;
	*unslice_freq_mhz = (unslice_freq * 16666) / 1000;
}

static uint32_t
gen8_report_reason(const uint32_t *report)
{
	return ((report[0] >> OAREPORT_REASON_SHIFT) &
		OAREPORT_REASON_MASK);
}

static const char *
gen8_read_report_reason(const uint32_t *report)
{
	uint32_t reason = gen8_report_reason(report);

	if (reason & (1<<0))
		return "timer";
	else if (reason & (1<<1))
	      return "internal trigger 1";
	else if (reason & (1<<2))
	      return "internal trigger 2";
	else if (reason & (1<<3))
	      return "context switch";
	else if (reason & (1<<4))
	      return "GO 1->0 transition (enter RC6)";
	else if (reason & (1<<5))
		return "[un]slice clock ratio change";
	else
		return "unknown";
}

static uint32_t
cs_timestamp_frequency(int fd)
{
	return xe_gt_list(drm_fd)->gt_list[0].reference_clock;
}

static uint64_t
cs_timebase_scale(uint32_t u32_delta)
{
	return ((uint64_t)u32_delta * NSEC_PER_SEC) / cs_timestamp_frequency(drm_fd);
}

static uint64_t
oa_timestamp(const uint32_t *report, enum xe_oa_format_name format)
{
	struct oa_format fmt = get_oa_format(format);

	return fmt.report_hdr_64bit ? *(uint64_t *)&report[2] : report[1];
}

static uint64_t
oa_timestamp_delta(const uint32_t *report1,
		   const uint32_t *report0,
		   enum xe_oa_format_name format)
{
	uint32_t width = intel_graphics_ver(devid) >= IP_VER(12, 55) ? 56 : 32;

	return elapsed_delta(oa_timestamp(report1, format),
			     oa_timestamp(report0, format), width);
}

static uint64_t
timebase_scale(uint64_t delta)
{
	return (delta * NSEC_PER_SEC) / intel_perf->devinfo.timestamp_frequency;
}

/* Returns: the largest OA exponent that will still result in a sampling period
 * less than or equal to the given @period.
 */
static int
max_oa_exponent_for_period_lte(uint64_t period)
{
	/* NB: timebase_scale() takes a uint64_t and an exponent of 30
	 * would already represent a period of ~3 minutes so there's
	 * really no need to consider higher exponents.
	 */
	for (int i = 0; i < 30; i++) {
		uint64_t oa_period = timebase_scale(2 << i);

		if (oa_period > period)
			return max(0, i - 1);
	}

	igt_assert(!"reached");
	return -1;
}

/* Return: the largest OA exponent that will still result in a sampling
 * frequency greater than the given @frequency.
 */
static int
max_oa_exponent_for_freq_gt(uint64_t frequency)
{
	uint64_t period = NSEC_PER_SEC / frequency;

	igt_assert_neq(period, 0);

	return max_oa_exponent_for_period_lte(period - 1);
}

static uint64_t
oa_exponent_to_ns(int exponent)
{
       return 1000000000ULL * (2ULL << exponent) / intel_perf->devinfo.timestamp_frequency;
}

static bool
oa_report_is_periodic(uint32_t oa_exponent, const uint32_t *report)
{
	if (gen8_report_reason(report) & OAREPORT_REASON_TIMER)
		return true;

	return false;
}

static bool
oa_report_ctx_is_valid(uint32_t *report)
{
	return report[0] & (1ul << 16);
}

static uint32_t
oa_report_get_ctx_id(uint32_t *report)
{
	if (!oa_report_ctx_is_valid(report))
		return 0xffffffff;
	return report[2];
}

static int
oar_unit_default_format(void)
{
	if (IS_DG2(devid) || IS_METEORLAKE(devid))
		return XE_OAR_FORMAT_A32u40_A4u32_B8_C8;

	return default_test_set->perf_oa_format;
}

/*
 * Temporary wrapper to distinguish mappings on !llc platforms,
 * where it seems cache over GEM_MMAP_OFFSET is not flushed before execution.
 */
static void *buf_map(int fd, struct intel_buf *buf, bool write)
{
	void *p;

	if (is_xe_device(fd)) {
		buf->ptr = xe_bo_map(fd, buf->handle, buf->surface[0].size);
		p = buf->ptr;
	} else {
		if (gem_has_llc(fd))
			p = intel_buf_cpu_map(buf, write);
		else
			p = intel_buf_device_map(buf, write);
	}
	return p;
}

static void
scratch_buf_memset(struct intel_buf *buf, int width, int height, uint32_t color)
{
	buf_map(buf_ops_get_fd(buf->bops), buf, true);

	for (int i = 0; i < width * height; i++)
		buf->ptr[i] = color;

	intel_buf_unmap(buf);
}

static void
scratch_buf_init(struct buf_ops *bops,
		 struct intel_buf *buf,
		 int width, int height,
		 uint32_t color)
{
	intel_buf_init(bops, buf, width, height, 32, 0,
		       I915_TILING_NONE, I915_COMPRESSION_NONE);
	scratch_buf_memset(buf, width, height, color);
}

static void
emit_report_perf_count(struct intel_bb *ibb,
		       struct intel_buf *dst,
		       int dst_offset,
		       uint32_t report_id)
{
	intel_bb_add_intel_buf(ibb, dst, true);

	intel_bb_out(ibb, GEN8_MI_REPORT_PERF_COUNT);
	intel_bb_emit_reloc(ibb, dst->handle,
			    I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION,
			    dst_offset, dst->addr.offset);
	intel_bb_out(ibb, report_id);
}

static uint64_t
gen8_read_40bit_a_counter(const uint32_t *report,
			  enum xe_oa_format_name fmt, int a_id)
{
	struct oa_format format = get_oa_format(fmt);
	uint8_t *a40_high = (((uint8_t *)report) + format.a40_high_off);
	uint32_t *a40_low = (uint32_t *)(((uint8_t *)report) +
					 format.a40_low_off);
	uint64_t high = (uint64_t)(a40_high[a_id]) << 32;

	return a40_low[a_id] | high;
}

static uint64_t
xehpsdv_read_64bit_a_counter(const uint32_t *report, enum xe_oa_format_name fmt, int a_id)
{
	struct oa_format format = get_oa_format(fmt);
	uint64_t *a64 = (uint64_t *)(((uint8_t *)report) + format.a64_off);

	return a64[a_id];
}

static uint64_t
gen8_40bit_a_delta(uint64_t value0, uint64_t value1)
{
	if (value0 > value1)
		return (1ULL << 40) + value1 - value0;
	else
		return value1 - value0;
}

static void
accumulate_uint32(size_t offset,
		  uint32_t *report0,
                  uint32_t *report1,
                  uint64_t *delta)
{
	uint32_t value0 = *(uint32_t *)(((uint8_t *)report0) + offset);
	uint32_t value1 = *(uint32_t *)(((uint8_t *)report1) + offset);

	*delta += (uint32_t)(value1 - value0);
}

static void
accumulate_uint40(int a_index,
                  uint32_t *report0,
                  uint32_t *report1,
		  enum xe_oa_format_name format,
                  uint64_t *delta)
{
	uint64_t value0 = gen8_read_40bit_a_counter(report0, format, a_index),
		 value1 = gen8_read_40bit_a_counter(report1, format, a_index);

	*delta += gen8_40bit_a_delta(value0, value1);
}

static void
accumulate_uint64(int a_index,
		  const uint32_t *report0,
		  const uint32_t *report1,
		  enum xe_oa_format_name format,
		  uint64_t *delta)
{
	uint64_t value0 = xehpsdv_read_64bit_a_counter(report0, format, a_index),
		 value1 = xehpsdv_read_64bit_a_counter(report1, format, a_index);

	*delta += (value1 - value0);
}

static void
accumulate_reports(struct accumulator *accumulator,
		   uint32_t *start,
		   uint32_t *end)
{
	struct oa_format format = get_oa_format(accumulator->format);
	uint64_t *deltas = accumulator->deltas;
	int idx = 0;

	if (intel_gen(devid) >= 8) {
		/* timestamp */
		deltas[idx] += oa_timestamp_delta(end, start, accumulator->format);
		idx++;

		/* clock cycles */
		deltas[idx] += oa_tick_delta(end, start, accumulator->format);
		idx++;
	} else {
		/* timestamp */
		accumulate_uint32(4, start, end, deltas + idx++);
	}

	for (int i = 0; i < format.n_a40; i++) {
		accumulate_uint40(i, start, end, accumulator->format,
				  deltas + idx++);
	}

	for (int i = 0; i < format.n_a64; i++) {
		accumulate_uint64(i, start, end, accumulator->format,
				  deltas + idx++);
	}

	for (int i = 0; i < format.n_a; i++) {
		accumulate_uint32(format.a_off + 4 * i,
				  start, end, deltas + idx++);
	}

	for (int i = 0; i < format.n_b; i++) {
		accumulate_uint32(format.b_off + 4 * i,
				  start, end, deltas + idx++);
	}

	for (int i = 0; i < format.n_c; i++) {
		accumulate_uint32(format.c_off + 4 * i,
				  start, end, deltas + idx++);
	}
}

static void
accumulator_print(struct accumulator *accumulator, const char *title)
{
	struct oa_format format = get_oa_format(accumulator->format);
	uint64_t *deltas = accumulator->deltas;
	int idx = 0;

	igt_debug("%s:\n", title);
	if (intel_gen(devid) >= 8) {
		igt_debug("\ttime delta = %"PRIu64"\n", deltas[idx++]);
		igt_debug("\tclock cycle delta = %"PRIu64"\n", deltas[idx++]);

		for (int i = 0; i < format.n_a40; i++)
			igt_debug("\tA%u = %"PRIu64"\n", i, deltas[idx++]);

		for (int i = 0; i < format.n_a64; i++)
			igt_debug("\tA64_%u = %"PRIu64"\n", i, deltas[idx++]);
	} else {
		igt_debug("\ttime delta = %"PRIu64"\n", deltas[idx++]);
	}

	for (int i = 0; i < format.n_a; i++) {
		int a_id = format.first_a + i;
		igt_debug("\tA%u = %"PRIu64"\n", a_id, deltas[idx++]);
	}

	for (int i = 0; i < format.n_a; i++)
		igt_debug("\tB%u = %"PRIu64"\n", i, deltas[idx++]);

	for (int i = 0; i < format.n_c; i++)
		igt_debug("\tC%u = %"PRIu64"\n", i, deltas[idx++]);
}

/* The TestOa metric set is designed so */
static void
gen8_sanity_check_test_oa_reports(const uint32_t *oa_report0,
				  const uint32_t *oa_report1,
				  enum xe_oa_format_name fmt)
{
	struct oa_format format = get_oa_format(fmt);
	uint64_t time_delta = timebase_scale(oa_timestamp_delta(oa_report1,
								oa_report0,
								fmt));
	uint64_t clock_delta = oa_tick_delta(oa_report1, oa_report0, fmt);
	uint64_t max_delta;
	uint64_t freq;
	uint32_t *rpt0_b = (uint32_t *)(((uint8_t *)oa_report0) +
					format.b_off);
	uint32_t *rpt1_b = (uint32_t *)(((uint8_t *)oa_report1) +
					format.b_off);
	uint32_t b;
	uint32_t ref;

	igt_debug("report type: %s->%s\n",
		  gen8_read_report_reason(oa_report0),
		  gen8_read_report_reason(oa_report1));

	freq = time_delta ? (clock_delta * 1000) / time_delta : 0;
	igt_debug("freq = %"PRIu64"\n", freq);

	igt_debug("clock delta = %"PRIu64"\n", clock_delta);

	max_delta = clock_delta * intel_perf->devinfo.n_eus;

	/* Gen8+ has some 40bit A counters... */
	for (int j = format.first_a40; j < format.n_a40 + format.first_a40; j++) {
		uint64_t value0 = gen8_read_40bit_a_counter(oa_report0, fmt, j);
		uint64_t value1 = gen8_read_40bit_a_counter(oa_report1, fmt, j);
		uint64_t delta = gen8_40bit_a_delta(value0, value1);

		if (undefined_a_counters[j])
			continue;

		igt_debug("A40_%d: delta = %"PRIu64"\n", j, delta);
		igt_assert_f(delta <= max_delta,
			     "A40_%d: delta = %"PRIu64", max_delta = %"PRIu64"\n",
			     j, delta, max_delta);
	}

	for (int j = 0; j < format.n_a64; j++) {
		uint64_t delta = 0;

		accumulate_uint64(j, oa_report0, oa_report1, fmt, &delta);

		if (undefined_a_counters[j])
			continue;

		igt_debug("A64_%d: delta = %"PRIu64"\n", format.first_a + j, delta);
		igt_assert_f(delta <= max_delta,
			     "A64_%d: delta = %"PRIu64", max_delta = %"PRIu64"\n",
			     format.first_a + j, delta, max_delta);
	}

	for (int j = 0; j < format.n_a; j++) {
		uint32_t *a0 = (uint32_t *)(((uint8_t *)oa_report0) +
					    format.a_off);
		uint32_t *a1 = (uint32_t *)(((uint8_t *)oa_report1) +
					    format.a_off);
		int a_id = format.first_a + j;
		uint32_t delta = a1[j] - a0[j];

		if (undefined_a_counters[a_id])
			continue;

		igt_debug("A%d: delta = %"PRIu32"\n", a_id, delta);
		igt_assert_f(delta <= max_delta,
			     "A%d: delta = %"PRIu32", max_delta = %"PRIu64"\n",
			     a_id, delta, max_delta);
	}

	/* The TestOa metric set defines all B counters to be a
	 * multiple of the gpu clock
	 */
	if (format.n_b && (format.oa_type == DRM_XE_OA_FMT_TYPE_OAG || format.oa_type == DRM_XE_OA_FMT_TYPE_OAR)) {
		if (clock_delta > 0) {
			b = rpt1_b[0] - rpt0_b[0];
			igt_debug("B0: delta = %"PRIu32"\n", b);
			igt_assert_eq(b, 0);

			b = rpt1_b[1] - rpt0_b[1];
			igt_debug("B1: delta = %"PRIu32"\n", b);
			igt_assert_eq(b, clock_delta);

			b = rpt1_b[2] - rpt0_b[2];
			igt_debug("B2: delta = %"PRIu32"\n", b);
			igt_assert_eq(b, clock_delta);

			b = rpt1_b[3] - rpt0_b[3];
			ref = clock_delta / 2;
			igt_debug("B3: delta = %"PRIu32"\n", b);
			igt_assert(b >= ref - 1 && b <= ref + 1);

			b = rpt1_b[4] - rpt0_b[4];
			ref = clock_delta / 3;
			igt_debug("B4: delta = %"PRIu32"\n", b);
			igt_assert(b >= ref - 1 && b <= ref + 1);

			b = rpt1_b[5] - rpt0_b[5];
			ref = clock_delta / 3;
			igt_debug("B5: delta = %"PRIu32"\n", b);
			igt_assert(b >= ref - 1 && b <= ref + 1);

			b = rpt1_b[6] - rpt0_b[6];
			ref = clock_delta / 6;
			igt_debug("B6: delta = %"PRIu32"\n", b);
			igt_assert(b >= ref - 1 && b <= ref + 1);

			b = rpt1_b[7] - rpt0_b[7];
			ref = clock_delta * 2 / 3;
			igt_debug("B7: delta = %"PRIu32"\n", b);
			igt_assert(b >= ref - 1 && b <= ref + 1);
		} else {
			for (int j = 0; j < format.n_b; j++) {
				b = rpt1_b[j] - rpt0_b[j];
				igt_debug("B%i: delta = %"PRIu32"\n", j, b);
				igt_assert_eq(b, 0);
			}
		}
	}

	for (int j = 0; j < format.n_c; j++) {
		uint32_t *c0 = (uint32_t *)(((uint8_t *)oa_report0) +
					    format.c_off);
		uint32_t *c1 = (uint32_t *)(((uint8_t *)oa_report1) +
					    format.c_off);
		uint32_t delta = c1[j] - c0[j];

		igt_debug("C%d: delta = %"PRIu32", max_delta=%"PRIu64"\n",
			  j, delta, max_delta);
		igt_assert_f(delta <= max_delta,
			     "C%d: delta = %"PRIu32", max_delta = %"PRIu64"\n",
			     j, delta, max_delta);
	}
}

static bool
init_sys_info(void)
{
	igt_assert_neq(devid, 0);

	intel_perf = intel_perf_for_fd(drm_fd, 0);
	igt_require(intel_perf);

	igt_debug("n_eu_slices: %"PRIu64"\n", intel_perf->devinfo.n_eu_slices);
	igt_debug("n_eu_sub_slices: %"PRIu64"\n", intel_perf->devinfo.n_eu_sub_slices);
	igt_debug("n_eus: %"PRIu64"\n", intel_perf->devinfo.n_eus);
	igt_debug("timestamp_frequency = %"PRIu64"\n",
		  intel_perf->devinfo.timestamp_frequency);
	igt_assert_neq(intel_perf->devinfo.timestamp_frequency, 0);

	read_report_ticks = gen8_read_report_ticks;
	sanity_check_reports = gen8_sanity_check_test_oa_reports;
	undefined_a_counters = gen8_undefined_a_counters;

	intel_perf_load_perf_configs(intel_perf, drm_fd);

	oa_exp_1_millisec = max_oa_exponent_for_period_lte(1000000);

	return true;
}

/* CAP_SYS_ADMIN is required to open system wide metrics, unless the system
 * control parameter dev.i915.perf_stream_paranoid == 0 */
static void
test_system_wide_paranoid(void)
{
	igt_fork(child, 1) {
		uint64_t properties[] = {
			DRM_XE_OA_PROPERTY_OA_UNIT_ID, 0,

			/* Include OA reports in samples */
			DRM_XE_OA_PROPERTY_SAMPLE_OA, true,

			/* OA unit configuration */
			DRM_XE_OA_PROPERTY_OA_METRIC_SET, default_test_set->perf_oa_metrics_set,
			DRM_XE_OA_PROPERTY_OA_FORMAT, __ff(default_test_set->perf_oa_format),
			DRM_XE_OA_PROPERTY_OA_EXPONENT, oa_exp_1_millisec,
		};
		struct drm_xe_oa_open_prop param = {
			.num_properties = ARRAY_SIZE(properties) / 2,
			.properties_ptr = to_user_pointer(properties),
		};

		write_u64_file("/proc/sys/dev/xe/perf_stream_paranoid", 1);

		igt_drop_root();

		xe_perf_ioctl_err(drm_fd, DRM_IOCTL_XE_PERF, DRM_XE_PERF_OP_STREAM_OPEN,
				  &param, EACCES);
	}

	igt_waitchildren();

	igt_fork(child, 1) {
		uint64_t properties[] = {
			DRM_XE_OA_PROPERTY_OA_UNIT_ID, 0,

			/* Include OA reports in samples */
			DRM_XE_OA_PROPERTY_SAMPLE_OA, true,

			/* OA unit configuration */
			DRM_XE_OA_PROPERTY_OA_METRIC_SET, default_test_set->perf_oa_metrics_set,
			DRM_XE_OA_PROPERTY_OA_FORMAT, __ff(default_test_set->perf_oa_format),
			DRM_XE_OA_PROPERTY_OA_EXPONENT, oa_exp_1_millisec,
		};
		struct drm_xe_oa_open_prop param = {
			.num_properties = ARRAY_SIZE(properties) / 2,
			.properties_ptr = to_user_pointer(properties),
		};
		write_u64_file("/proc/sys/dev/xe/perf_stream_paranoid", 0);

		igt_drop_root();

		stream_fd = __perf_open(drm_fd, &param, false);
		__perf_close(stream_fd);
	}

	igt_waitchildren();

	/* leave in paranoid state */
	write_u64_file("/proc/sys/dev/xe/perf_stream_paranoid", 1);
}

static void
test_invalid_oa_metric_set_id(void)
{
	uint64_t properties[] = {
		DRM_XE_OA_PROPERTY_OA_UNIT_ID, 0,

		/* Include OA reports in samples */
		DRM_XE_OA_PROPERTY_SAMPLE_OA, true,

		/* OA unit configuration */
		DRM_XE_OA_PROPERTY_OA_FORMAT, __ff(default_test_set->perf_oa_format),
		DRM_XE_OA_PROPERTY_OA_EXPONENT, oa_exp_1_millisec,
		DRM_XE_OA_PROPERTY_OA_METRIC_SET, UINT64_MAX,
	};
	struct drm_xe_oa_open_prop param = {
		.num_properties = ARRAY_SIZE(properties) / 2,
		.properties_ptr = to_user_pointer(properties),
	};

	xe_perf_ioctl_err(drm_fd, DRM_IOCTL_XE_PERF, DRM_XE_PERF_OP_STREAM_OPEN, &param, EINVAL);

	properties[ARRAY_SIZE(properties) - 1] = 0; /* ID 0 is also be reserved as invalid */
	xe_perf_ioctl_err(drm_fd, DRM_IOCTL_XE_PERF, DRM_XE_PERF_OP_STREAM_OPEN, &param, EINVAL);

	/* Check that we aren't just seeing false positives... */
	properties[ARRAY_SIZE(properties) - 1] = default_test_set->perf_oa_metrics_set;
	stream_fd = __perf_open(drm_fd, &param, false);
	__perf_close(stream_fd);

	/* There's no valid default OA metric set ID... */
	param.num_properties--;
	xe_perf_ioctl_err(drm_fd, DRM_IOCTL_XE_PERF, DRM_XE_PERF_OP_STREAM_OPEN, &param, EINVAL);
}

static void
test_invalid_oa_format_id(void)
{
	uint64_t properties[] = {
		DRM_XE_OA_PROPERTY_OA_UNIT_ID, 0,

		/* Include OA reports in samples */
		DRM_XE_OA_PROPERTY_SAMPLE_OA, true,

		/* OA unit configuration */
		DRM_XE_OA_PROPERTY_OA_METRIC_SET, default_test_set->perf_oa_metrics_set,
		DRM_XE_OA_PROPERTY_OA_EXPONENT, oa_exp_1_millisec,
		DRM_XE_OA_PROPERTY_OA_FORMAT, UINT64_MAX, /* No __ff() here */
	};
	struct drm_xe_oa_open_prop param = {
		.num_properties = ARRAY_SIZE(properties) / 2,
		.properties_ptr = to_user_pointer(properties),
	};

	xe_perf_ioctl_err(drm_fd, DRM_IOCTL_XE_PERF, DRM_XE_PERF_OP_STREAM_OPEN, &param, EINVAL);

	properties[ARRAY_SIZE(properties) - 1] = __ff(0); /* ID 0 is also be reserved as invalid */
	xe_perf_ioctl_err(drm_fd, DRM_IOCTL_XE_PERF, DRM_XE_PERF_OP_STREAM_OPEN, &param, EINVAL);

	/* Check that we aren't just seeing false positives... */
	properties[ARRAY_SIZE(properties) - 1] = __ff(default_test_set->perf_oa_format);
	stream_fd = __perf_open(drm_fd, &param, false);
	__perf_close(stream_fd);
#if 0 /* Can't do this if we have struct not propoerty key/value pairs */
	/* There's no valid default OA format... */
	param.num_properties--;
	xe_perf_ioctl_err(drm_fd, DRM_IOCTL_XE_PERF, XE_PERF_STREAM_OPEN, &param, EINVAL);
#endif
}

static void
test_missing_sample_flags(void)
{
	uint64_t properties[] = {
		DRM_XE_OA_PROPERTY_OA_UNIT_ID, 0,

		/* No _PROP_SAMPLE_xyz flags */

		/* OA unit configuration */
		DRM_XE_OA_PROPERTY_OA_METRIC_SET, default_test_set->perf_oa_metrics_set,
		DRM_XE_OA_PROPERTY_OA_EXPONENT, oa_exp_1_millisec,
		DRM_XE_OA_PROPERTY_OA_FORMAT, __ff(default_test_set->perf_oa_format),
	};
	struct drm_xe_oa_open_prop param = {
		.num_properties = ARRAY_SIZE(properties) / 2,
		.properties_ptr = to_user_pointer(properties),
	};

	xe_perf_ioctl_err(drm_fd, DRM_IOCTL_XE_PERF, DRM_XE_PERF_OP_STREAM_OPEN, &param, EINVAL);
}

static void
read_2_oa_reports(int format_id,
		  int exponent,
		  uint32_t *oa_report0,
		  uint32_t *oa_report1,
		  bool timer_only)
{
	size_t format_size = get_oa_format(format_id).size;
	size_t sample_size = (sizeof(struct drm_xe_oa_record_header) +
			      format_size);
	const struct drm_xe_oa_record_header *header;
	uint32_t exponent_mask = (1 << (exponent + 1)) - 1;

	/* Note: we allocate a large buffer so that each read() iteration
	 * should scrape *all* pending records.
	 *
	 * The largest buffer the OA unit supports is 16MB.
	 *
	 * Being sure we are fetching all buffered reports allows us to
	 * potentially throw away / skip all reports whenever we see
	 * a _REPORT_LOST notification as a way of being sure are
	 * measurements aren't skewed by a lost report.
	 *
	 * Note: that is is useful for some tests but also not something
	 * applications would be expected to resort to. Lost reports are
	 * somewhat unpredictable but typically don't pose a problem - except
	 * to indicate that the OA unit may be over taxed if lots of reports
	 * are being lost.
	 */
	int max_reports = MAX_OA_BUF_SIZE / format_size;
	int buf_size = sample_size * max_reports * 1.5;
	uint8_t *buf = malloc(buf_size);
	int n = 0;

	for (int i = 0; i < 1000; i++) {
		ssize_t len;

		while ((len = read(stream_fd, buf, buf_size)) < 0 &&
		       errno == EINTR)
			;

		igt_assert(len > 0);
		igt_debug("read %d bytes\n", (int)len);

		for (size_t offset = 0; offset < len; offset += header->size) {
			const uint32_t *report;

			header = (void *)(buf + offset);

			igt_assert_eq(header->pad, 0); /* Reserved */

			/* Currently the only test that should ever expect to
			 * see a _BUFFER_LOST error is the buffer_fill test,
			 * otherwise something bad has probably happened...
			 */
			igt_assert_neq(header->type, DRM_XE_OA_RECORD_OA_BUFFER_LOST);

			/* At high sampling frequencies the OA HW might not be
			 * able to cope with all write requests and will notify
			 * us that a report was lost. We restart our read of
			 * two sequential reports due to the timeline blip this
			 * implies
			 */
			if (header->type == DRM_XE_OA_RECORD_OA_REPORT_LOST) {
				igt_debug("read restart: OA trigger collision / report lost\n");
				n = 0;

				/* XXX: break, because we don't know where
				 * within the series of already read reports
				 * there could be a blip from the lost report.
				 */
				break;
			}

			/* Currently the only other record type expected is a
			 * _SAMPLE. Notably this test will need updating if
			 * i915-perf is extended in the future with additional
			 * record types.
			 */
			igt_assert_eq(header->type, DRM_XE_OA_RECORD_SAMPLE);

			igt_assert_eq(header->size, sample_size);

			report = (const void *)(header + 1);
			dump_report(report, format_size / 4, "oa-formats");

			igt_debug("read report: reason = %x, timestamp = %"PRIx64", exponent mask=%x\n",
				  report[0], oa_timestamp(report, format_id), exponent_mask);

			/* Don't expect zero for timestamps */
			igt_assert_neq_u64(oa_timestamp(report, format_id), 0);

			if (timer_only) {
				if (!oa_report_is_periodic(exponent, report)) {
					igt_debug("skipping non timer report\n");
					continue;
				}
			}

			if (n++ == 0)
				memcpy(oa_report0, report, format_size);
			else {
				memcpy(oa_report1, report, format_size);
				free(buf);
				return;
			}
		}
	}

	free(buf);

	igt_assert(!"reached");
}

static void
open_and_read_2_oa_reports(int format_id,
			   int exponent,
			   uint32_t *oa_report0,
			   uint32_t *oa_report1,
			   bool timer_only,
			   const struct intel_execution_engine2 *e)
{
	struct intel_perf_metric_set *test_set = metric_set(e);
	uint64_t properties[] = {
		DRM_XE_OA_PROPERTY_OA_UNIT_ID, 0,

		/* Include OA reports in samples */
		DRM_XE_OA_PROPERTY_SAMPLE_OA, true,

		/* OA unit configuration */
		DRM_XE_OA_PROPERTY_OA_METRIC_SET, test_set->perf_oa_metrics_set,
		DRM_XE_OA_PROPERTY_OA_FORMAT, __ff(format_id),
		DRM_XE_OA_PROPERTY_OA_EXPONENT, exponent,
		DRM_XE_OA_PROPERTY_OA_ENGINE_INSTANCE, e->instance,

	};
	struct drm_xe_oa_open_prop param = {
		.num_properties = has_param_class_instance() ?
				  ARRAY_SIZE(properties) / 2 :
				  (ARRAY_SIZE(properties) / 2) - 2,
		.properties_ptr = to_user_pointer(properties),
	};

	stream_fd = __perf_open(drm_fd, &param, false);
	set_fd_flags(stream_fd, O_CLOEXEC);

	read_2_oa_reports(format_id, exponent,
			  oa_report0, oa_report1, timer_only);

	__perf_close(stream_fd);
}

static void
print_reports(uint32_t *oa_report0, uint32_t *oa_report1, int fmt)
{
	struct oa_format format = get_oa_format(fmt);
	uint64_t ts0 = oa_timestamp(oa_report0, fmt);
	uint64_t ts1 = oa_timestamp(oa_report1, fmt);

	igt_debug("TIMESTAMP: 1st = %"PRIu64", 2nd = %"PRIu64", delta = %"PRIu64"\n",
		  ts0, ts1, ts1 - ts0);

	if (IS_HASWELL(devid) && format.n_c == 0) {
		igt_debug("CLOCK = N/A\n");
	} else {
		uint64_t clock0 = read_report_ticks(oa_report0, fmt);
		uint64_t clock1 = read_report_ticks(oa_report1, fmt);

		igt_debug("CLOCK: 1st = %"PRIu64", 2nd = %"PRIu64", delta = %"PRIu64"\n",
			  clock0, clock1, clock1 - clock0);
	}

	if (intel_gen(devid) >= 8) {
		uint32_t slice_freq0, slice_freq1, unslice_freq0, unslice_freq1;
		const char *reason0 = gen8_read_report_reason(oa_report0);
		const char *reason1 = gen8_read_report_reason(oa_report1);

		igt_debug("CTX ID: 1st = %"PRIu32", 2nd = %"PRIu32"\n",
			  oa_report0[2], oa_report1[2]);

		gen8_read_report_clock_ratios(oa_report0,
					      &slice_freq0, &unslice_freq0);
		gen8_read_report_clock_ratios(oa_report1,
					      &slice_freq1, &unslice_freq1);

		igt_debug("SLICE CLK: 1st = %umhz, 2nd = %umhz, delta = %d\n",
			  slice_freq0, slice_freq1,
			  ((int)slice_freq1 - (int)slice_freq0));
		igt_debug("UNSLICE CLK: 1st = %umhz, 2nd = %umhz, delta = %d\n",
			  unslice_freq0, unslice_freq1,
			  ((int)unslice_freq1 - (int)unslice_freq0));

		igt_debug("REASONS: 1st = \"%s\", 2nd = \"%s\"\n", reason0, reason1);
	}

	/* Gen8+ has some 40bit A counters... */
	for (int j = 0; j < format.n_a40; j++) {
		uint64_t value0 = gen8_read_40bit_a_counter(oa_report0, fmt, j);
		uint64_t value1 = gen8_read_40bit_a_counter(oa_report1, fmt, j);
		uint64_t delta = gen8_40bit_a_delta(value0, value1);

		if (undefined_a_counters[j])
			continue;

		igt_debug("A%d: 1st = %"PRIu64", 2nd = %"PRIu64", delta = %"PRIu64"\n",
			  j, value0, value1, delta);
	}

	for (int j = 0; j < format.n_a64; j++) {
		uint64_t value0 = xehpsdv_read_64bit_a_counter(oa_report0, fmt, j);
		uint64_t value1 = xehpsdv_read_64bit_a_counter(oa_report1, fmt, j);
		uint64_t delta = value1 - value0;

		if (undefined_a_counters[j])
			continue;

		igt_debug("A_64%d: 1st = %"PRIu64", 2nd = %"PRIu64", delta = %"PRIu64"\n",
			  format.first_a + j, value0, value1, delta);
	}

	for (int j = 0; j < format.n_a; j++) {
		uint32_t *a0 = (uint32_t *)(((uint8_t *)oa_report0) +
					    format.a_off);
		uint32_t *a1 = (uint32_t *)(((uint8_t *)oa_report1) +
					    format.a_off);
		int a_id = format.first_a + j;
		uint32_t delta = a1[j] - a0[j];

		if (undefined_a_counters[a_id])
			continue;

		igt_debug("A%d: 1st = %"PRIu32", 2nd = %"PRIu32", delta = %"PRIu32"\n",
			  a_id, a0[j], a1[j], delta);
	}

	for (int j = 0; j < format.n_b; j++) {
		uint32_t *b0 = (uint32_t *)(((uint8_t *)oa_report0) +
					    format.b_off);
		uint32_t *b1 = (uint32_t *)(((uint8_t *)oa_report1) +
					    format.b_off);
		uint32_t delta = b1[j] - b0[j];

		igt_debug("B%d: 1st = %"PRIu32", 2nd = %"PRIu32", delta = %"PRIu32"\n",
			  j, b0[j], b1[j], delta);
	}

	for (int j = 0; j < format.n_c; j++) {
		uint32_t *c0 = (uint32_t *)(((uint8_t *)oa_report0) +
					    format.c_off);
		uint32_t *c1 = (uint32_t *)(((uint8_t *)oa_report1) +
					    format.c_off);
		uint32_t delta = c1[j] - c0[j];

		igt_debug("C%d: 1st = %"PRIu32", 2nd = %"PRIu32", delta = %"PRIu32"\n",
			  j, c0[j], c1[j], delta);
	}
}

/* Debug function, only useful when reports don't make sense. */
#if 0
static void
print_report(uint32_t *report, int fmt)
{
	struct oa_format format = get_oa_format(fmt);

	igt_debug("TIMESTAMP: %"PRIu64"\n", oa_timestamp(report, fmt));

	if (IS_HASWELL(devid) && format.n_c == 0) {
		igt_debug("CLOCK = N/A\n");
	} else {
		uint64_t clock = read_report_ticks(report, fmt);

		igt_debug("CLOCK: %"PRIu64"\n", clock);
	}

	if (intel_gen(devid) >= 8) {
		uint32_t slice_freq, unslice_freq;
		const char *reason = gen8_read_report_reason(report);

		gen8_read_report_clock_ratios(report, &slice_freq, &unslice_freq);

		igt_debug("SLICE CLK: %umhz\n", slice_freq);
		igt_debug("UNSLICE CLK: %umhz\n", unslice_freq);
		igt_debug("REASON: \"%s\"\n", reason);
		igt_debug("CTX ID: %"PRIu32"/%"PRIx32"\n", report[2], report[2]);
	}

	/* Gen8+ has some 40bit A counters... */
	for (int j = 0; j < format.n_a40; j++) {
		uint64_t value = gen8_read_40bit_a_counter(report, fmt, j);

		if (undefined_a_counters[j])
			continue;

		igt_debug("A%d: %"PRIu64"\n", j, value);
	}

	for (int j = 0; j < format.n_a; j++) {
		uint32_t *a = (uint32_t *)(((uint8_t *)report) +
					   format.a_off);
		int a_id = format.first_a + j;

		if (undefined_a_counters[a_id])
			continue;

		igt_debug("A%d: %"PRIu32"\n", a_id, a[j]);
	}

	for (int j = 0; j < format.n_b; j++) {
		uint32_t *b = (uint32_t *)(((uint8_t *)report) +
					   format.b_off);

		igt_debug("B%d: %"PRIu32"\n", j, b[j]);
	}

	for (int j = 0; j < format.n_c; j++) {
		uint32_t *c = (uint32_t *)(((uint8_t *)report) +
					   format.c_off);

		igt_debug("C%d: %"PRIu32"\n", j, c[j]);
	}
}
#endif

static bool
oa_unit_supports_engine(int oa_type, const struct intel_execution_engine2 *e)
{
	switch (oa_type) {
	case DRM_XE_OA_FMT_TYPE_OAM:
	case DRM_XE_OA_FMT_TYPE_OAM_MPEC:
		return e->class == DRM_XE_ENGINE_CLASS_VIDEO_DECODE ||
		       e->class == DRM_XE_ENGINE_CLASS_VIDEO_ENHANCE;
	case DRM_XE_OA_FMT_TYPE_OAG:
	case DRM_XE_OA_FMT_TYPE_OAR:
		return e->class == DRM_XE_ENGINE_CLASS_RENDER;
	case DRM_XE_OA_FMT_TYPE_OAC:
		return e->class == DRM_XE_ENGINE_CLASS_COMPUTE;
	case DRM_XE_OA_FMT_TYPE_PEC:
		return e->class == DRM_XE_ENGINE_CLASS_RENDER ||
		       e->class == DRM_XE_ENGINE_CLASS_COMPUTE;
	default:
		return false;
	}

}

static void
test_oa_formats(const struct intel_execution_engine2 *e)
{
	for (int i = 0; i < XE_OA_FORMAT_MAX; i++) {
		struct oa_format format = get_oa_format(i);
		uint32_t oa_report0[format.size / 4];
		uint32_t oa_report1[format.size / 4];

		if (!format.name) /* sparse, indexed by ID */
			continue;

		/* FIXME: change oa_unit_supports_engine to handle all format.oa_type */
		if (!IS_LUNARLAKE(devid) && !oa_unit_supports_engine(format.oa_type, e))
			continue;

		igt_debug("Checking OA format %s\n", format.name);

		open_and_read_2_oa_reports(i,
					   oa_exp_1_millisec,
					   oa_report0,
					   oa_report1,
					   false, /* timer reports only */
					   e);

		print_reports(oa_report0, oa_report1, i);
		sanity_check_reports(oa_report0, oa_report1, i);
	}
}


enum load {
	LOW,
	HIGH
};

#define LOAD_HELPER_PAUSE_USEC 500

static struct load_helper {
	int devid;
	struct buf_ops *bops;
	uint32_t context_id;
	uint32_t vm;
	struct intel_bb *ibb;
	enum load load;
	bool exit;
	struct igt_helper_process igt_proc;
	struct intel_buf src, dst;
} lh = { 0, };

static void load_helper_signal_handler(int sig)
{
	if (sig == SIGUSR2)
		lh.load = lh.load == LOW ? HIGH : LOW;
	else
		lh.exit = true;
}

static void load_helper_set_load(enum load load)
{
	igt_assert(lh.igt_proc.running);

	if (lh.load == load)
		return;

	lh.load = load;
	kill(lh.igt_proc.pid, SIGUSR2);
}

static void load_helper_run(enum load load)
{
	if (!render_copy)
		return;

	/*
	 * FIXME fork helpers won't get cleaned up when started from within a
	 * subtest, so handle the case where it sticks around a bit too long.
	 */
	if (lh.igt_proc.running) {
		load_helper_set_load(load);
		return;
	}

	lh.load = load;

	igt_fork_helper(&lh.igt_proc) {
		signal(SIGUSR1, load_helper_signal_handler);
		signal(SIGUSR2, load_helper_signal_handler);

		while (!lh.exit) {
			render_copy(lh.ibb,
				    &lh.src, 0, 0, 1920, 1080,
				    &lh.dst, 0, 0);

			intel_bb_sync(lh.ibb);

			/* Lower the load by pausing after every submitted
			 * write. */
			if (lh.load == LOW)
				usleep(LOAD_HELPER_PAUSE_USEC);
		}
	}
}

static void load_helper_stop(void)
{
	if (!render_copy)
		return;

	kill(lh.igt_proc.pid, SIGUSR1);
	igt_assert(igt_wait_helper(&lh.igt_proc) == 0);
}

static void load_helper_init(void)
{
	if (!render_copy) {
		igt_info("Running test without render_copy\n");
		return;
	}

	lh.devid = intel_get_drm_devid(drm_fd);

	/* MI_STORE_DATA can only use GTT address on gen4+/g33 and needs
	 * snoopable mem on pre-gen6. Hence load-helper only works on gen6+, but
	 * that's also all we care about for the rps testcase*/
	igt_assert(intel_gen(lh.devid) >= 6);

	lh.bops = buf_ops_create(drm_fd);

	if (is_xe_device(drm_fd)) {
		lh.vm = xe_vm_create(drm_fd, 0, 0);
		lh.context_id = xe_exec_queue_create(drm_fd, lh.vm, &xe_engine(drm_fd, 0)->instance, 0);
	} else {
		lh.context_id = gem_context_create(drm_fd);
	}
	igt_assert_neq(lh.context_id, 0xffffffff);

	lh.ibb = intel_bb_create_with_context(drm_fd, lh.context_id, lh.vm, NULL, BATCH_SZ);

	scratch_buf_init(lh.bops, &lh.dst, 1920, 1080, 0);
	scratch_buf_init(lh.bops, &lh.src, 1920, 1080, 0);
}

static void load_helper_fini(void)
{
	if (!render_copy)
		return;

	if (lh.igt_proc.running)
		load_helper_stop();

	intel_buf_close(lh.bops, &lh.src);
	intel_buf_close(lh.bops, &lh.dst);
	intel_bb_destroy(lh.ibb);
	if (is_xe_device(drm_fd)) {
		xe_exec_queue_destroy(drm_fd, lh.context_id);
		xe_vm_destroy(drm_fd, lh.vm);
	} else {
		gem_context_destroy(drm_fd, lh.context_id);
	}
	buf_ops_destroy(lh.bops);
}

static bool expected_report_timing_delta(uint32_t delta, uint32_t expected_delta)
{
	/*
	 * On ICL, the OA unit appears to be a bit more relaxed about
	 * its timing for emitting OA reports (often missing the
	 * deadline by 1 timestamp).
	 */
	if (IS_ICELAKE(devid))
		return delta <= (expected_delta + 3);
	else
		return delta <= expected_delta;
}

static void
test_oa_exponents(const struct intel_execution_engine2 *e)
{
	struct intel_perf_metric_set *test_set = metric_set(e);
	uint64_t fmt = test_set->perf_oa_format;

	load_helper_init();
	load_helper_run(HIGH);

	/* It's asking a lot to sample with a 160 nanosecond period and the
	 * test can fail due to buffer overflows if it wasn't possible to
	 * keep up, so we don't start from an exponent of zero...
	 */
	for (int exponent = 5; exponent < 20; exponent++) {
		uint64_t properties[] = {
			DRM_XE_OA_PROPERTY_OA_UNIT_ID, 0,

			/* Include OA reports in samples */
			DRM_XE_OA_PROPERTY_SAMPLE_OA, true,

			/* OA unit configuration */
			DRM_XE_OA_PROPERTY_OA_METRIC_SET, test_set->perf_oa_metrics_set,
			DRM_XE_OA_PROPERTY_OA_FORMAT, __ff(fmt),
			DRM_XE_OA_PROPERTY_OA_EXPONENT, exponent,
			DRM_XE_OA_PROPERTY_OA_ENGINE_INSTANCE, e->instance,
		};
		struct drm_xe_oa_open_prop param = {
			.num_properties = has_param_class_instance() ?
					  ARRAY_SIZE(properties) / 2 :
					  (ARRAY_SIZE(properties) / 2) - 2,
			.properties_ptr = to_user_pointer(properties),
		};
		uint64_t expected_timestamp_delta = 2ULL << exponent;
		size_t format_size = get_oa_format(fmt).size;
		size_t sample_size = (sizeof(struct drm_xe_oa_record_header) +
				      format_size);
		int max_reports = MAX_OA_BUF_SIZE / format_size;
		int buf_size = sample_size * max_reports * 1.5;
		uint8_t *buf = calloc(1, buf_size);
		int ret, n_timer_reports = 0;
		uint32_t matches = 0;
#define NUM_TIMER_REPORTS 30
		uint32_t *reports = malloc(NUM_TIMER_REPORTS * format_size);
		uint32_t *timer_reports = reports;

		igt_debug("testing OA exponent %d,"
			  " expected ts delta = %"PRIu64" (%"PRIu64"ns/%.2fus/%.2fms)\n",
			  exponent, expected_timestamp_delta,
			  oa_exponent_to_ns(exponent),
			  oa_exponent_to_ns(exponent) / 1000.0,
			  oa_exponent_to_ns(exponent) / (1000.0 * 1000.0));

		stream_fd = __perf_open(drm_fd, &param, true /* prevent_pm */);

		while (n_timer_reports < NUM_TIMER_REPORTS) {
			struct drm_xe_oa_record_header *header;

			while ((ret = read(stream_fd, buf, buf_size)) < 0 &&
			       errno == EINTR)
				;

			/* igt_debug(" > read %i bytes\n", ret); */

			/* We should never have no data. */
			igt_assert(ret > 0);

			for (int offset = 0;
			     offset < ret && n_timer_reports < NUM_TIMER_REPORTS;
			     offset += header->size) {
				uint32_t *report;

				header = (void *)(buf + offset);

				if (header->type == DRM_XE_OA_RECORD_OA_BUFFER_LOST) {
					igt_assert(!"reached");
					break;
				}

				if (header->type == DRM_XE_OA_RECORD_OA_REPORT_LOST)
					igt_debug("report loss\n");

				if (header->type != DRM_XE_OA_RECORD_SAMPLE)
					continue;

				report = (void *)(header + 1);

				if (!oa_report_is_periodic(exponent, report))
					continue;

				memcpy(timer_reports, report, format_size);
				n_timer_reports++;
				timer_reports += (format_size / 4);
			}
		}

		__perf_close(stream_fd);

		igt_debug("report%04i ts=%"PRIx64" hw_id=0x%08x\n", 0,
			  oa_timestamp(&reports[0], fmt),
			  oa_report_get_ctx_id(&reports[0]));
		for (int i = 1; i < n_timer_reports; i++) {
			uint64_t delta = oa_timestamp_delta(&reports[i],
							    &reports[i - 1],
							    fmt);

			igt_debug("report%04i ts=%"PRIx64" hw_id=0x%08x delta=%"PRIu64" %s\n", i,
				  oa_timestamp(&reports[i], fmt),
				  oa_report_get_ctx_id(&reports[i]),
				  delta, expected_report_timing_delta(delta,
								      expected_timestamp_delta) ? "" : "******");

			matches += expected_report_timing_delta(delta,expected_timestamp_delta);
		}

		igt_debug("matches=%u/%u\n", matches, n_timer_reports - 1);

		/*
		 * Expect half the reports to match the timing
		 * expectation. The results are quite erratic because
		 * the condition under which the HW reaches
		 * expectations depends on memory controller pressure
		 * etc...
		 */
		igt_assert_lte(n_timer_reports / 2, matches);

		free(reports);
	}

	load_helper_stop();
	load_helper_fini();
}

/* The OA exponent selects a timestamp counter bit to trigger reports on.
 *
 * With a 64bit timestamp and least significant bit approx == 80ns then the MSB
 * equates to > 40 thousand years and isn't exposed via the i915 perf interface.
 *
 * The max exponent exposed is expected to be 31, which is still a fairly
 * ridiculous period (>5min) but is the maximum exponent where it's still
 * possible to use periodic sampling as a means for tracking the overflow of
 * 32bit OA report timestamps.
 */
static void
test_invalid_oa_exponent(void)
{
	uint64_t properties[] = {
		/* Include OA reports in samples */
		DRM_XE_OA_PROPERTY_SAMPLE_OA, true,

		/* OA unit configuration */
		DRM_XE_OA_PROPERTY_OA_METRIC_SET, default_test_set->perf_oa_metrics_set,
		DRM_XE_OA_PROPERTY_OA_FORMAT, __ff(default_test_set->perf_oa_format),
		DRM_XE_OA_PROPERTY_OA_EXPONENT, 31, /* maximum exponent expected
						       to be accepted */
	};
	struct drm_xe_oa_open_prop param = {
		.num_properties = ARRAY_SIZE(properties) / 2,
		.properties_ptr = to_user_pointer(properties),
	};

	stream_fd = __perf_open(drm_fd, &param, false);

	__perf_close(stream_fd);

	for (int i = 32; i < 65; i++) {
		properties[7] = i;
		xe_perf_ioctl_err(drm_fd, DRM_IOCTL_XE_PERF, DRM_XE_PERF_OP_STREAM_OPEN,
				  &param, EINVAL);
	}
}

/* The lowest periodic sampling exponent equates to a period of 160 nanoseconds
 * or a frequency of 6.25MHz which is only possible to request as root by
 * default. By default the maximum OA sampling rate is 100KHz
 */
static void
test_low_oa_exponent_permissions(void)
{
	int max_freq = read_u64_file("/proc/sys/dev/xe/oa_max_sample_rate");
	int bad_exponent = max_oa_exponent_for_freq_gt(max_freq);
	int ok_exponent = bad_exponent + 1;
	uint64_t properties[] = {
		/* Include OA reports in samples */
		DRM_XE_OA_PROPERTY_SAMPLE_OA, true,

		/* OA unit configuration */
		DRM_XE_OA_PROPERTY_OA_METRIC_SET, default_test_set->perf_oa_metrics_set,
		DRM_XE_OA_PROPERTY_OA_FORMAT, __ff(default_test_set->perf_oa_format),
		DRM_XE_OA_PROPERTY_OA_EXPONENT, bad_exponent,
	};
	struct drm_xe_oa_open_prop param = {
		.num_properties = ARRAY_SIZE(properties) / 2,
		.properties_ptr = to_user_pointer(properties),
	};
	uint64_t oa_period, oa_freq;

	igt_assert_eq(max_freq, 100000);

	/* Avoid EACCES errors opening a stream without CAP_SYS_ADMIN */
	write_u64_file("/proc/sys/dev/xe/perf_stream_paranoid", 0);

	igt_fork(child, 1) {
		igt_drop_root();

		xe_perf_ioctl_err(drm_fd, DRM_IOCTL_XE_PERF, DRM_XE_PERF_OP_STREAM_OPEN,
				  &param, EACCES);
	}

	igt_waitchildren();

	properties[7] = ok_exponent;

	igt_fork(child, 1) {
		igt_drop_root();

		stream_fd = __perf_open(drm_fd, &param, false);
		__perf_close(stream_fd);
	}

	igt_waitchildren();

	oa_period = timebase_scale(2 << ok_exponent);
	oa_freq = NSEC_PER_SEC / oa_period;
	write_u64_file("/proc/sys/dev/xe/oa_max_sample_rate", oa_freq - 100);

	igt_fork(child, 1) {
		igt_drop_root();

		xe_perf_ioctl_err(drm_fd, DRM_IOCTL_XE_PERF, DRM_XE_PERF_OP_STREAM_OPEN,
				  &param, EACCES);
	}

	igt_waitchildren();

	/* restore the defaults */
	write_u64_file("/proc/sys/dev/xe/oa_max_sample_rate", 100000);
	write_u64_file("/proc/sys/dev/xe/perf_stream_paranoid", 1);
}

static int64_t
get_time(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);

	return ts.tv_sec * 1000000000 + ts.tv_nsec;
}

/* Note: The interface doesn't currently provide strict guarantees or control
 * over the upper bound for how long it might take for a POLLIN event after
 * some OA report is written by the OA unit.
 *
 * The plan is to add a property later that gives some control over the maximum
 * latency, but for now we expect it is tuned for a fairly low latency
 * suitable for applications wanting to provide live feedback for captured
 * metrics.
 *
 * At the time of writing this test the driver was using a fixed 200Hz hrtimer
 * regardless of the OA sampling exponent.
 *
 * There is no lower bound since a stream configured for periodic sampling may
 * still contain other automatically triggered reports.
 *
 * What we try and check for here is that blocking reads don't return EAGAIN
 * and that we aren't spending any significant time burning the cpu in
 * kernelspace.
 */
static void
test_blocking(uint64_t requested_oa_period,
	      bool set_kernel_hrtimer,
	      uint64_t kernel_hrtimer,
	      const struct intel_execution_engine2 *e)
{
	int oa_exponent = max_oa_exponent_for_period_lte(requested_oa_period);
	uint64_t oa_period = oa_exponent_to_ns(oa_exponent);
	uint64_t props[DRM_XE_OA_PROPERTY_MAX * 2];
	uint64_t *idx = props;
	struct drm_xe_oa_open_prop param;
	uint8_t buf[1024 * 1024];
	struct tms start_times;
	struct tms end_times;
	int64_t user_ns, kernel_ns;
	int64_t tick_ns = 1000000000 / sysconf(_SC_CLK_TCK);
	int64_t test_duration_ns = tick_ns * 100;

	int max_iterations = (test_duration_ns / oa_period) + 2;
	int n_extra_iterations = 0;
	int perf_fd;

	/* It's a bit tricky to put a lower limit here, but we expect a
	 * relatively low latency for seeing reports, while we don't currently
	 * give any control over this in the api.
	 *
	 * We assume a maximum latency of 6 millisecond to deliver a POLLIN and
	 * read() after a new sample is written (46ms per iteration) considering
	 * the knowledge that that the driver uses a 200Hz hrtimer (5ms period)
	 * to check for data and giving some time to read().
	 */
	int min_iterations = (test_duration_ns / (oa_period + kernel_hrtimer + kernel_hrtimer / 5));

	int64_t start, end;
	int n = 0;
	struct intel_perf_metric_set *test_set = metric_set(e);

	ADD_PROPS(props, idx, SAMPLE_OA, true);
	ADD_PROPS(props, idx, OA_METRIC_SET, test_set->perf_oa_metrics_set);
	ADD_PROPS(props, idx, OA_FORMAT, __ff(test_set->perf_oa_format));
	ADD_PROPS(props, idx, OA_EXPONENT, oa_exponent);
	ADD_PROPS(props, idx, OA_DISABLED, true);

	if (has_param_class_instance()) {
		ADD_PROPS(props, idx, OA_UNIT_ID, 0);
		ADD_PROPS(props, idx, OA_ENGINE_INSTANCE, e->instance);
	}

	param.num_properties = (idx - props) / 2;
	param.properties_ptr = to_user_pointer(props);

	perf_fd = __perf_open(drm_fd, &param, true /* prevent_pm */);
        set_fd_flags(perf_fd, O_CLOEXEC);

	times(&start_times);

	igt_debug("tick length = %dns, test duration = %"PRIu64"ns, min iter. = %d,"
		  " estimated max iter. = %d, oa_period = %s\n",
		  (int)tick_ns, test_duration_ns,
		  min_iterations, max_iterations,
		  pretty_print_oa_period(oa_period));

	/* In the loop we perform blocking polls while the HW is sampling at
	 * ~25Hz, with the expectation that we spend most of our time blocked
	 * in the kernel, and shouldn't be burning cpu cycles in the kernel in
	 * association with this process (verified by looking at stime before
	 * and after loop).
	 *
	 * We're looking to assert that less than 1% of the test duration is
	 * spent in the kernel dealing with polling and read()ing.
	 *
	 * The test runs for a relatively long time considering the very low
	 * resolution of stime in ticks of typically 10 milliseconds. Since we
	 * don't know the fractional part of tick values we read from userspace
	 * so our minimum threshold needs to be >= one tick since any
	 * measurement might really be +- tick_ns (assuming we effectively get
	 * floor(real_stime)).
	 *
	 * We Loop for 1000 x tick_ns so one tick corresponds to 0.1%
	 *
	 * Also enable the stream just before poll/read to minimize
	 * the error delta.
	 */
	start = get_time();
	do_ioctl(perf_fd, DRM_XE_PERF_IOCTL_ENABLE, 0);
	for (/* nop */; ((end = get_time()) - start) < test_duration_ns; /* nop */) {
		struct drm_xe_oa_record_header *header;
		bool timer_report_read = false;
		bool non_timer_report_read = false;
		int ret;

		while ((ret = read(perf_fd, buf, sizeof(buf))) < 0 &&
		       errno == EINTR)
			;

		igt_assert(ret > 0);

		/* For Haswell reports don't contain a well defined reason
		 * field we so assume all reports to be 'periodic'. For gen8+
		 * we want to to consider that the HW automatically writes some
		 * non periodic reports (e.g. on context switch) which might
		 * lead to more successful read()s than expected due to
		 * periodic sampling and we don't want these extra reads to
		 * cause the test to fail...
		 */
		if (intel_gen(devid) >= 8) {
			for (int offset = 0; offset < ret; offset += header->size) {
				header = (void *)(buf + offset);

				if (header->type == DRM_XE_OA_RECORD_SAMPLE) {
					uint32_t *report = (void *)(header + 1);

					if (oa_report_is_periodic(oa_exponent,
								  report))
						timer_report_read = true;
					else
						non_timer_report_read = true;
				}
			}
		}

		if (non_timer_report_read && !timer_report_read)
			n_extra_iterations++;

		n++;
	}

	times(&end_times);

	/* Using nanosecond units is fairly silly here, given the tick in-
	 * precision - ah well, it's consistent with the get_time() units.
	 */
	user_ns = (end_times.tms_utime - start_times.tms_utime) * tick_ns;
	kernel_ns = (end_times.tms_stime - start_times.tms_stime) * tick_ns;

	igt_debug("%d blocking reads during test with %"PRIu64" Hz OA sampling (expect no more than %d)\n",
		  n, NSEC_PER_SEC / oa_period, max_iterations);
	igt_debug("%d extra iterations seen, not related to periodic sampling (e.g. context switches)\n",
		  n_extra_iterations);
	igt_debug("time in userspace = %"PRIu64"ns (+-%dns) (start utime = %d, end = %d)\n",
		  user_ns, (int)tick_ns,
		  (int)start_times.tms_utime, (int)end_times.tms_utime);
	igt_debug("time in kernelspace = %"PRIu64"ns (+-%dns) (start stime = %d, end = %d)\n",
		  kernel_ns, (int)tick_ns,
		  (int)start_times.tms_stime, (int)end_times.tms_stime);

	/* With completely broken blocking (but also not returning an error) we
	 * could end up with an open loop,
	 */
	igt_assert(n <= (max_iterations + n_extra_iterations));

	/* Make sure the driver is reporting new samples with a reasonably
	 * low latency...
	 */
	igt_assert(n > (min_iterations + n_extra_iterations));

	if (!set_kernel_hrtimer)
		igt_assert(kernel_ns <= (test_duration_ns / 100ull));

	__perf_close(perf_fd);
}

static void
test_polling(uint64_t requested_oa_period,
	     bool set_kernel_hrtimer,
	     uint64_t kernel_hrtimer,
	     const struct intel_execution_engine2 *e)
{
	int oa_exponent = max_oa_exponent_for_period_lte(requested_oa_period);
	uint64_t oa_period = oa_exponent_to_ns(oa_exponent);
	uint64_t props[DRM_XE_OA_PROPERTY_MAX * 2];
	uint64_t *idx = props;
	struct drm_xe_oa_open_prop param;
	uint8_t buf[1024 * 1024];
	struct tms start_times;
	struct tms end_times;
	int64_t user_ns, kernel_ns;
	int64_t tick_ns = 1000000000 / sysconf(_SC_CLK_TCK);
	int64_t test_duration_ns = tick_ns * 100;

	int max_iterations = (test_duration_ns / oa_period) + 2;
	int n_extra_iterations = 0;

	/* It's a bit tricky to put a lower limit here, but we expect a
	 * relatively low latency for seeing reports.
	 *
	 * We assume a maximum latency of kernel_hrtimer + some margin
	 * to deliver a POLLIN and read() after a new sample is
	 * written (40ms + hrtimer + margin per iteration) considering
	 * the knowledge that that the driver uses a 200Hz hrtimer
	 * (5ms period) to check for data and giving some time to
	 * read().
	 */
	int min_iterations = (test_duration_ns / (oa_period + (kernel_hrtimer + kernel_hrtimer / 5)));
	int64_t start, end;
	int n = 0;
	struct intel_perf_metric_set *test_set = metric_set(e);

	ADD_PROPS(props, idx, SAMPLE_OA, true);
	ADD_PROPS(props, idx, OA_METRIC_SET, test_set->perf_oa_metrics_set);
	ADD_PROPS(props, idx, OA_FORMAT, __ff(test_set->perf_oa_format));
	ADD_PROPS(props, idx, OA_EXPONENT, oa_exponent);
	ADD_PROPS(props, idx, OA_DISABLED, true);

	if (has_param_class_instance()) {
		ADD_PROPS(props, idx, OA_UNIT_ID, 0);
		ADD_PROPS(props, idx, OA_ENGINE_INSTANCE, e->instance);
	}

	param.num_properties = (idx - props) / 2;
	param.properties_ptr = to_user_pointer(props);

	stream_fd = __perf_open(drm_fd, &param, true /* prevent_pm */);
	set_fd_flags(stream_fd, O_CLOEXEC | O_NONBLOCK);

	times(&start_times);

	igt_debug("tick length = %dns, oa period = %s, "
		  "test duration = %"PRIu64"ns, min iter. = %d, max iter. = %d\n",
		  (int)tick_ns, pretty_print_oa_period(oa_period), test_duration_ns,
		  min_iterations, max_iterations);

	/* In the loop we perform blocking polls while the HW is sampling at
	 * ~25Hz, with the expectation that we spend most of our time blocked
	 * in the kernel, and shouldn't be burning cpu cycles in the kernel in
	 * association with this process (verified by looking at stime before
	 * and after loop).
	 *
	 * We're looking to assert that less than 1% of the test duration is
	 * spent in the kernel dealing with polling and read()ing.
	 *
	 * The test runs for a relatively long time considering the very low
	 * resolution of stime in ticks of typically 10 milliseconds. Since we
	 * don't know the fractional part of tick values we read from userspace
	 * so our minimum threshold needs to be >= one tick since any
	 * measurement might really be +- tick_ns (assuming we effectively get
	 * floor(real_stime)).
	 *
	 * We Loop for 1000 x tick_ns so one tick corresponds to 0.1%
	 *
	 * Also enable the stream just before poll/read to minimize
	 * the error delta.
	 */
	start = get_time();
	do_ioctl(stream_fd, DRM_XE_PERF_IOCTL_ENABLE, 0);
	for (/* nop */; ((end = get_time()) - start) < test_duration_ns; /* nop */) {
		struct pollfd pollfd = { .fd = stream_fd, .events = POLLIN };
		struct drm_xe_oa_record_header *header;
		bool timer_report_read = false;
		bool non_timer_report_read = false;
		int ret;

		while ((ret = poll(&pollfd, 1, -1)) < 0 &&
		       errno == EINTR)
			;
		igt_assert_eq(ret, 1);
		igt_assert(pollfd.revents & POLLIN);

		while ((ret = read(stream_fd, buf, sizeof(buf))) < 0 &&
		       errno == EINTR)
			;

		/* Don't expect to see EAGAIN if we've had a POLLIN event
		 *
		 * XXX: actually this is technically overly strict since we do
		 * knowingly allow false positive POLLIN events. At least in
		 * the future when supporting context filtering of metrics for
		 * Gen8+ handled in the kernel then POLLIN events may be
		 * delivered when we know there are pending reports to process
		 * but before we've done any filtering to know for certain that
		 * any reports are destined to be copied to userspace.
		 *
		 * Still, for now it's a reasonable sanity check.
		 */
		if (ret < 0)
			igt_debug("Unexpected error when reading after poll = %d\n", errno);
		igt_assert_neq(ret, -1);

		/* For Haswell reports don't contain a well defined reason
		 * field we so assume all reports to be 'periodic'. For gen8+
		 * we want to to consider that the HW automatically writes some
		 * non periodic reports (e.g. on context switch) which might
		 * lead to more successful read()s than expected due to
		 * periodic sampling and we don't want these extra reads to
		 * cause the test to fail...
		 */
		if (intel_gen(devid) >= 8) {
			for (int offset = 0; offset < ret; offset += header->size) {
				header = (void *)(buf + offset);

				if (header->type == DRM_XE_OA_RECORD_SAMPLE) {
					uint32_t *report = (void *)(header + 1);

					if (oa_report_is_periodic(oa_exponent, report))
						timer_report_read = true;
					else
						non_timer_report_read = true;
				}
			}
		}

		if (non_timer_report_read && !timer_report_read)
			n_extra_iterations++;

		/* At this point, after consuming pending reports (and hoping
		 * the scheduler hasn't stopped us for too long) we now expect
		 * EAGAIN on read. While this works most of the times, there are
		 * some rare failures when the OA period passed to this test is
		 * very small (say 500 us) and that results in some valid
		 * reports here. To weed out those rare occurences we assert
		 * only if the OA period is >= 40 ms because 40 ms has withstood
		 * the test of time on most platforms (ref: subtest: polling).
		 */
		while ((ret = read(stream_fd, buf, sizeof(buf))) < 0 &&
		       errno == EINTR)
			;

		if (requested_oa_period >= 40000000) {
			igt_assert_eq(ret, -1);
			igt_assert_eq(errno, EAGAIN);
		}

		n++;
	}

	times(&end_times);

	/* Using nanosecond units is fairly silly here, given the tick in-
	 * precision - ah well, it's consistent with the get_time() units.
	 */
	user_ns = (end_times.tms_utime - start_times.tms_utime) * tick_ns;
	kernel_ns = (end_times.tms_stime - start_times.tms_stime) * tick_ns;

	igt_debug("%d non-blocking reads during test with %"PRIu64" Hz OA sampling (expect no more than %d)\n",
		  n, NSEC_PER_SEC / oa_period, max_iterations);
	igt_debug("%d extra iterations seen, not related to periodic sampling (e.g. context switches)\n",
		  n_extra_iterations);
	igt_debug("time in userspace = %"PRIu64"ns (+-%dns) (start utime = %d, end = %d)\n",
		  user_ns, (int)tick_ns,
		  (int)start_times.tms_utime, (int)end_times.tms_utime);
	igt_debug("time in kernelspace = %"PRIu64"ns (+-%dns) (start stime = %d, end = %d)\n",
		  kernel_ns, (int)tick_ns,
		  (int)start_times.tms_stime, (int)end_times.tms_stime);

	/* With completely broken blocking while polling (but still somehow
	 * reporting a POLLIN event) we could end up with an open loop.
	 */
	igt_assert(n <= (max_iterations + n_extra_iterations));

	/* Make sure the driver is reporting new samples with a reasonably
	 * low latency...
	 */
	igt_assert(n > (min_iterations + n_extra_iterations));

	if (!set_kernel_hrtimer)
		igt_assert(kernel_ns <= (test_duration_ns / 100ull));

	__perf_close(stream_fd);
}

static void test_polling_small_buf(void)
{
	int oa_exponent = max_oa_exponent_for_period_lte(40 * 1000); /* 40us */
	uint64_t properties[] = {
		DRM_XE_OA_PROPERTY_OA_UNIT_ID, 0,

		/* Include OA reports in samples */
		DRM_XE_OA_PROPERTY_SAMPLE_OA, true,

		/* OA unit configuration */
		DRM_XE_OA_PROPERTY_OA_METRIC_SET, default_test_set->perf_oa_metrics_set,
		DRM_XE_OA_PROPERTY_OA_FORMAT, __ff(default_test_set->perf_oa_format),
		DRM_XE_OA_PROPERTY_OA_EXPONENT, oa_exponent,
		DRM_XE_OA_PROPERTY_OA_DISABLED, true,
	};
	struct drm_xe_oa_open_prop param = {
		.num_properties = ARRAY_SIZE(properties) / 2,
		.properties_ptr = to_user_pointer(properties),
	};
	uint32_t test_duration = 80 * 1000 * 1000;
	int sample_size = (sizeof(struct drm_xe_oa_record_header) +
			   get_oa_format(default_test_set->perf_oa_format).size);
	int n_expected_reports = test_duration / oa_exponent_to_ns(oa_exponent);
	int n_expect_read_bytes = n_expected_reports * sample_size;
	struct timespec ts = {};
	int n_bytes_read = 0;
	uint32_t n_polls = 0;

	stream_fd = __perf_open(drm_fd, &param, true /* prevent_pm */);
	set_fd_flags(stream_fd, O_CLOEXEC | O_NONBLOCK);
	do_ioctl(stream_fd, DRM_XE_PERF_IOCTL_ENABLE, 0);

	while (igt_nsec_elapsed(&ts) < test_duration) {
		struct pollfd pollfd = { .fd = stream_fd, .events = POLLIN };

		ppoll(&pollfd, 1, NULL, NULL);
		if (pollfd.revents & POLLIN) {
			uint8_t buf[1024];
			int ret;

			ret = read(stream_fd, buf, sizeof(buf));
			if (ret >= 0)
				n_bytes_read += ret;
		}

		n_polls++;
	}

	igt_info("Read %d expected %d (%.2f%% of the expected number), polls=%u\n",
		 n_bytes_read, n_expect_read_bytes,
		 n_bytes_read * 100.0f / n_expect_read_bytes,
		 n_polls);

	__perf_close(stream_fd);

	igt_assert(abs(n_expect_read_bytes - n_bytes_read) <
		   0.20 * n_expect_read_bytes);
}

static int
num_valid_reports_captured(struct drm_xe_oa_open_prop *param,
			   int64_t *duration_ns)
{
	uint8_t buf[1024 * 1024];
	int64_t start, end;
	int num_reports = 0;

	igt_debug("Expected duration = %"PRId64"\n", *duration_ns);

	stream_fd = __perf_open(drm_fd, param, true);

	start = get_time();
	do_ioctl(stream_fd, DRM_XE_PERF_IOCTL_ENABLE, 0);
	for (/* nop */; ((end = get_time()) - start) < *duration_ns; /* nop */) {
		struct drm_xe_oa_record_header *header;
		int ret;

		while ((ret = read(stream_fd, buf, sizeof(buf))) < 0 &&
		       errno == EINTR)
			;

		igt_assert(ret > 0);

		for (int offset = 0; offset < ret; offset += header->size) {
			header = (void *)(buf + offset);

			if (header->type == DRM_XE_OA_RECORD_SAMPLE) {
				uint32_t *report = (void *)(header + 1);

				if (gen8_report_reason(report) & OAREPORT_REASON_TIMER)
					num_reports++;
			}
		}
	}
	__perf_close(stream_fd);

	*duration_ns = end - start;

	igt_debug("Actual duration = %"PRIu64"\n", *duration_ns);

	return num_reports;
}

static void
gen12_test_oa_tlb_invalidate(const struct intel_execution_engine2 *e)
{
	int oa_exponent = max_oa_exponent_for_period_lte(30000000);
	struct intel_perf_metric_set *test_set = metric_set(e);
	uint64_t properties[] = {
		DRM_XE_OA_PROPERTY_OA_UNIT_ID, 0,
		DRM_XE_OA_PROPERTY_SAMPLE_OA, true,

		DRM_XE_OA_PROPERTY_OA_METRIC_SET, test_set->perf_oa_metrics_set,
		DRM_XE_OA_PROPERTY_OA_FORMAT, __ff(test_set->perf_oa_format),
		DRM_XE_OA_PROPERTY_OA_EXPONENT, oa_exponent,
		DRM_XE_OA_PROPERTY_OA_DISABLED, true,
		DRM_XE_OA_PROPERTY_OA_ENGINE_INSTANCE, e->instance,
	};
	struct drm_xe_oa_open_prop param = {
		.num_properties = has_param_class_instance() ?
				  ARRAY_SIZE(properties) / 2 :
				  (ARRAY_SIZE(properties) / 2) - 2,
		.properties_ptr = to_user_pointer(properties),
	};
	int num_reports1, num_reports2, num_expected_reports;
	int64_t duration;

	/* Capture reports for 5 seconds twice and then make sure you get around
	 * the same number of reports. In the case of failure, the number of
	 * reports will vary largely since the beginning of the OA buffer
	 * will have invalid entries.
	 */
	duration = 5LL * NSEC_PER_SEC;
	num_reports1 = num_valid_reports_captured(&param, &duration);
	num_expected_reports = duration / oa_exponent_to_ns(oa_exponent);
	igt_debug("expected num reports = %d\n", num_expected_reports);
	igt_debug("actual num reports = %d\n", num_reports1);
	igt_assert(num_reports1 > 0.95 * num_expected_reports);

	duration = 5LL * NSEC_PER_SEC;
	num_reports2 = num_valid_reports_captured(&param, &duration);
	num_expected_reports = duration / oa_exponent_to_ns(oa_exponent);
	igt_debug("expected num reports = %d\n", num_expected_reports);
	igt_debug("actual num reports = %d\n", num_reports2);
	igt_assert(num_reports2 > 0.95 * num_expected_reports);
}


static void
test_buffer_fill(const struct intel_execution_engine2 *e)
{
	/* ~5 micro second period */
	int oa_exponent = max_oa_exponent_for_period_lte(5000);
	uint64_t oa_period = oa_exponent_to_ns(oa_exponent);
	struct intel_perf_metric_set *test_set = metric_set(e);
	uint64_t fmt = test_set->perf_oa_format;
	uint64_t properties[] = {
		DRM_XE_OA_PROPERTY_OA_UNIT_ID, 0,

		/* Include OA reports in samples */
		DRM_XE_OA_PROPERTY_SAMPLE_OA, true,

		/* OA unit configuration */
		DRM_XE_OA_PROPERTY_OA_METRIC_SET, test_set->perf_oa_metrics_set,
		DRM_XE_OA_PROPERTY_OA_FORMAT, __ff(fmt),
		DRM_XE_OA_PROPERTY_OA_EXPONENT, oa_exponent,
		DRM_XE_OA_PROPERTY_OA_ENGINE_INSTANCE, e->instance,
	};
	struct drm_xe_oa_open_prop param = {
		.num_properties = has_param_class_instance() ?
				  ARRAY_SIZE(properties) / 2 :
				  (ARRAY_SIZE(properties) / 2) - 2,
		.properties_ptr = to_user_pointer(properties),
	};
	struct drm_xe_oa_record_header *header;
	size_t report_size = get_oa_format(fmt).size;
	int buf_size = 65536 * (report_size + sizeof(struct drm_xe_oa_record_header));
	uint8_t *buf = malloc(buf_size);
	int len;
	size_t oa_buf_size = MAX_OA_BUF_SIZE;
	int n_full_oa_reports = oa_buf_size / report_size;
	uint64_t fill_duration = n_full_oa_reports * oa_period;
	uint32_t *last_periodic_report = malloc(report_size);

	igt_assert(fill_duration < 1000000000);

	stream_fd = __perf_open(drm_fd, &param, true /* prevent_pm */);
        set_fd_flags(stream_fd, O_CLOEXEC);

	for (int i = 0; i < 5; i++) {
		bool overflow_seen;
		uint32_t n_periodic_reports;
		uint32_t first_timestamp = 0, last_timestamp = 0;

		do_ioctl(stream_fd, DRM_XE_PERF_IOCTL_ENABLE, 0);

		nanosleep(&(struct timespec){ .tv_sec = 0,
					      .tv_nsec = fill_duration * 1.25 },
			  NULL);

		while ((len = read(stream_fd, buf, buf_size)) == -1 && errno == EINTR)
			;

		igt_assert_neq(len, -1);

		overflow_seen = false;
		for (int offset = 0; offset < len; offset += header->size) {
			header = (void *)(buf + offset);

			if (header->type == DRM_XE_OA_RECORD_OA_BUFFER_LOST)
				overflow_seen = true;
		}

		/* Overrun mode is disabled in the kernel for Xe2+ */
		if (!xe_relax_checks(drm_fd))
			igt_assert_eq(overflow_seen, true);

		do_ioctl(stream_fd, DRM_XE_PERF_IOCTL_DISABLE, 0);

		igt_debug("fill_duration = %"PRIu64"ns, oa_exponent = %u\n",
			  fill_duration, oa_exponent);

		do_ioctl(stream_fd, DRM_XE_PERF_IOCTL_ENABLE, 0);

		nanosleep(&(struct timespec){ .tv_sec = 0,
					.tv_nsec = fill_duration / 2 },
			NULL);

		n_periodic_reports = 0;

		/* Because of the race condition between notification of new
		 * reports and reports landing in memory, we need to rely on
		 * timestamps to figure whether we've read enough of them.
		 */
		while (((last_timestamp - first_timestamp) * oa_period) < (fill_duration / 2)) {

			igt_debug("dts=%u elapsed=%"PRIu64" duration=%"PRIu64"\n",
				  last_timestamp - first_timestamp,
				  (last_timestamp - first_timestamp) * oa_period,
				  fill_duration / 2);

			while ((len = read(stream_fd, buf, buf_size)) == -1 && errno == EINTR)
				;

			igt_assert_neq(len, -1);

			for (int offset = 0; offset < len; offset += header->size) {
				uint32_t *report;

				header = (void *) (buf + offset);
				report = (void *) (header + 1);

				switch (header->type) {
				case DRM_XE_OA_RECORD_OA_REPORT_LOST:
					igt_debug("report loss, trying again\n");
					break;
				case DRM_XE_OA_RECORD_SAMPLE:
					igt_debug(" > report ts=%"PRIu64""
						  " ts_delta_last_periodic=%"PRIu64" is_timer=%i ctx_id=%8x nb_periodic=%u\n",
						  oa_timestamp(report, fmt),
						  n_periodic_reports > 0 ?  oa_timestamp_delta(report, last_periodic_report, fmt) : 0,
						  oa_report_is_periodic(oa_exponent, report),
						  oa_report_get_ctx_id(report),
						  n_periodic_reports);

					if (first_timestamp == 0)
						first_timestamp = oa_timestamp(report, fmt);
					last_timestamp = oa_timestamp(report, fmt);

					if (((last_timestamp - first_timestamp) * oa_period) < (fill_duration / 2))
						break;

					if (oa_report_is_periodic(oa_exponent, report)) {
						memcpy(last_periodic_report, report, report_size);
						n_periodic_reports++;
					}
					break;
				case DRM_XE_OA_RECORD_OA_BUFFER_LOST:
					igt_assert(!"unexpected overflow");
					break;
				}
			}
		}

		do_ioctl(stream_fd, DRM_XE_PERF_IOCTL_DISABLE, 0);

		igt_debug("first ts = %u, last ts = %u\n", first_timestamp, last_timestamp);

		igt_debug("%f < %zu < %f\n",
			  report_size * n_full_oa_reports * 0.45,
			  n_periodic_reports * report_size,
			  report_size * n_full_oa_reports * 0.55);

		igt_assert(n_periodic_reports * report_size >
			   report_size * n_full_oa_reports * 0.45);
		igt_assert(n_periodic_reports * report_size <
			   report_size * n_full_oa_reports * 0.55);
	}

	free(last_periodic_report);
	free(buf);

	__perf_close(stream_fd);
}

static void
test_non_zero_reason(const struct intel_execution_engine2 *e)
{
	/* ~20 micro second period */
	int oa_exponent = max_oa_exponent_for_period_lte(20000);
	struct intel_perf_metric_set *test_set = metric_set(e);
	uint64_t fmt = test_set->perf_oa_format;
	size_t report_size = get_oa_format(fmt).size;
	uint64_t properties[] = {
		DRM_XE_OA_PROPERTY_OA_UNIT_ID, 0,

		/* Include OA reports in samples */
		DRM_XE_OA_PROPERTY_SAMPLE_OA, true,

		/* OA unit configuration */
		DRM_XE_OA_PROPERTY_OA_METRIC_SET, test_set->perf_oa_metrics_set,
		DRM_XE_OA_PROPERTY_OA_FORMAT, __ff(fmt),
		DRM_XE_OA_PROPERTY_OA_EXPONENT, oa_exponent,
		DRM_XE_OA_PROPERTY_OA_ENGINE_INSTANCE, e->instance,
	};
	struct drm_xe_oa_open_prop param = {
		.num_properties = has_param_class_instance() ?
				  ARRAY_SIZE(properties) / 2 :
				  (ARRAY_SIZE(properties) / 2) - 2,
		.properties_ptr = to_user_pointer(properties),
	};
	struct drm_xe_oa_record_header *header;
	uint32_t buf_size = 3 * 65536 * (report_size + sizeof(struct drm_xe_oa_record_header));
	uint8_t *buf = malloc(buf_size);
	uint32_t total_len = 0, reports_lost;
	const uint32_t *last_report;
	int len;

	igt_assert(buf);

	igt_debug("Ready to read about %u bytes\n", buf_size);

	load_helper_init();
	load_helper_run(HIGH);

	stream_fd = __perf_open(drm_fd, &param, true /* prevent_pm */);
        set_fd_flags(stream_fd, O_CLOEXEC);

	while (total_len < (buf_size - sizeof(struct drm_xe_oa_record_header)) &&
	       ((len = read(stream_fd, &buf[total_len], buf_size - total_len)) > 0 ||
		(len == -1 && errno == EINTR))) {
		if (len > 0)
			total_len += len;
	}

	__perf_close(stream_fd);

	load_helper_stop();
	load_helper_fini();

	igt_debug("Got %u bytes\n", total_len);

	last_report = NULL;
	reports_lost = 0;
	for (uint32_t offset = 0; offset < total_len; offset += header->size) {
		header = (void *) (buf + offset);

		switch (header->type) {
		case DRM_XE_OA_RECORD_OA_REPORT_LOST:
			reports_lost++;
			break;
		case DRM_XE_OA_RECORD_SAMPLE: {
			const uint32_t *report = (void *) (header + 1);
			uint32_t reason = (report[0] >> OAREPORT_REASON_SHIFT) &
				OAREPORT_REASON_MASK;

			igt_assert_neq(reason, 0);

			if (last_report)
				sanity_check_reports(last_report, report, fmt);

			last_report = report;
			break;
		}
		case DRM_XE_OA_RECORD_OA_BUFFER_LOST:
			igt_assert(!"unexpected overflow");
			break;
		}
	}

	igt_debug("Got %u report lost events\n", reports_lost);

	free(buf);
}

static void
test_enable_disable(const struct intel_execution_engine2 *e)
{
	/* ~5 micro second period */
	int oa_exponent = max_oa_exponent_for_period_lte(5000);
	uint64_t oa_period = oa_exponent_to_ns(oa_exponent);
	struct intel_perf_metric_set *test_set = metric_set(e);
	uint64_t fmt = test_set->perf_oa_format;
	uint64_t properties[] = {
		DRM_XE_OA_PROPERTY_OA_UNIT_ID, 0,

		/* Include OA reports in samples */
		DRM_XE_OA_PROPERTY_SAMPLE_OA, true,

		/* OA unit configuration */
		DRM_XE_OA_PROPERTY_OA_METRIC_SET, test_set->perf_oa_metrics_set,
		DRM_XE_OA_PROPERTY_OA_FORMAT, __ff(fmt),
		DRM_XE_OA_PROPERTY_OA_EXPONENT, oa_exponent,
		DRM_XE_OA_PROPERTY_OA_DISABLED, true,
		DRM_XE_OA_PROPERTY_OA_ENGINE_INSTANCE, e->instance,
	};
	struct drm_xe_oa_open_prop param = {
		.num_properties = has_param_class_instance() ?
				  ARRAY_SIZE(properties) / 2 :
				  (ARRAY_SIZE(properties) / 2) - 2,
		.properties_ptr = to_user_pointer(properties),
	};
	size_t report_size = get_oa_format(fmt).size;
	int buf_size = 65536 * (report_size + sizeof(struct drm_xe_oa_record_header));
	uint8_t *buf = malloc(buf_size);
	size_t oa_buf_size = MAX_OA_BUF_SIZE;
	int n_full_oa_reports = oa_buf_size / report_size;
	uint64_t fill_duration = n_full_oa_reports * oa_period;
	uint32_t *last_periodic_report = malloc(report_size);

	load_helper_init();
	load_helper_run(HIGH);

	stream_fd = __perf_open(drm_fd, &param, true /* prevent_pm */);
        set_fd_flags(stream_fd, O_CLOEXEC);

	for (int i = 0; i < 5; i++) {
		int len;
		uint32_t n_periodic_reports;
		struct drm_xe_oa_record_header *header;
		uint64_t first_timestamp = 0, last_timestamp = 0;

		/* Giving enough time for an overflow might help catch whether
		 * the OA unit has been enabled even if the driver might at
		 * least avoid copying reports while disabled.
		 */
		nanosleep(&(struct timespec){ .tv_sec = 0,
					      .tv_nsec = fill_duration * 1.25 },
			  NULL);

		while ((len = read(stream_fd, buf, buf_size)) == -1 && errno == EINTR)
			;

		igt_assert_eq(len, -1);
		igt_assert_eq(errno, EIO);

		do_ioctl(stream_fd, DRM_XE_PERF_IOCTL_ENABLE, 0);

		nanosleep(&(struct timespec){ .tv_sec = 0,
					      .tv_nsec = fill_duration / 2 },
			NULL);

		n_periodic_reports = 0;

		/* Because of the race condition between notification of new
		 * reports and reports landing in memory, we need to rely on
		 * timestamps to figure whether we've read enough of them.
		 */
		while (((last_timestamp - first_timestamp) * oa_period) < (fill_duration / 2)) {

			while ((len = read(stream_fd, buf, buf_size)) == -1 && errno == EINTR)
				;

			igt_assert_neq(len, -1);

			for (int offset = 0; offset < len; offset += header->size) {
				uint32_t *report;

				header = (void *) (buf + offset);
				report = (void *) (header + 1);

				switch (header->type) {
				case DRM_XE_OA_RECORD_OA_REPORT_LOST:
					break;
				case DRM_XE_OA_RECORD_SAMPLE:
					if (first_timestamp == 0)
						first_timestamp = oa_timestamp(report, fmt);
					last_timestamp = oa_timestamp(report, fmt);

					igt_debug(" > report ts=%"PRIx64""
						  " ts_delta_last_periodic=%s%"PRIu64""
						  " is_timer=%i ctx_id=0x%8x\n",
						  oa_timestamp(report, fmt),
						  oa_report_is_periodic(oa_exponent, report) ? " " : "*",
						  n_periodic_reports > 0 ?  oa_timestamp_delta(report, last_periodic_report, fmt) : 0,
						  oa_report_is_periodic(oa_exponent, report),
						  oa_report_get_ctx_id(report));

					if (((last_timestamp - first_timestamp) * oa_period) < (fill_duration / 2))
						break;

					if (oa_report_is_periodic(oa_exponent, report)) {
						memcpy(last_periodic_report, report, report_size);

						/* We want to measure only the
						 * periodic reports, ctx-switch
						 * might inflate the content of
						 * the buffer and skew or
						 * measurement.
						 */
						n_periodic_reports++;
					}
					break;
				case DRM_XE_OA_RECORD_OA_BUFFER_LOST:
					igt_assert(!"unexpected overflow");
					break;
				}
			}

		}

		do_ioctl(stream_fd, DRM_XE_PERF_IOCTL_DISABLE, 0);

		igt_debug("first ts = %lu, last ts = %lu\n", first_timestamp, last_timestamp);

		igt_debug("%f < %zu < %f\n",
			  report_size * n_full_oa_reports * 0.45,
			  n_periodic_reports * report_size,
			  report_size * n_full_oa_reports * 0.55);

		igt_assert((n_periodic_reports * report_size) >
			   (report_size * n_full_oa_reports * 0.45));
		igt_assert((n_periodic_reports * report_size) <
			   report_size * n_full_oa_reports * 0.55);


		/* It's considered an error to read a stream while it's disabled
		 * since it would block indefinitely...
		 */
		len = read(stream_fd, buf, buf_size);

		igt_assert_eq(len, -1);
		igt_assert_eq(errno, EIO);
	}

	free(last_periodic_report);
	free(buf);

	__perf_close(stream_fd);

	load_helper_stop();
	load_helper_fini();
}

static void
test_short_reads(void)
{
	int oa_exponent = max_oa_exponent_for_period_lte(5000);
	uint64_t properties[] = {
		DRM_XE_OA_PROPERTY_OA_UNIT_ID, 0,

		/* Include OA reports in samples */
		DRM_XE_OA_PROPERTY_SAMPLE_OA, true,

		/* OA unit configuration */
		DRM_XE_OA_PROPERTY_OA_METRIC_SET, default_test_set->perf_oa_metrics_set,
		DRM_XE_OA_PROPERTY_OA_FORMAT, __ff(default_test_set->perf_oa_format),
		DRM_XE_OA_PROPERTY_OA_EXPONENT, oa_exponent,
	};
	struct drm_xe_oa_open_prop param = {
		.num_properties = ARRAY_SIZE(properties) / 2,
		.properties_ptr = to_user_pointer(properties),
	};
	size_t record_size = 256 + sizeof(struct drm_xe_oa_record_header);
	size_t page_size = sysconf(_SC_PAGE_SIZE);
	int zero_fd = open("/dev/zero", O_RDWR|O_CLOEXEC);
	uint8_t *pages = mmap(NULL, page_size * 2,
			      PROT_READ|PROT_WRITE, MAP_PRIVATE, zero_fd, 0);
	struct drm_xe_oa_record_header *header;
	int ret;

	igt_assert_neq(zero_fd, -1);
	close(zero_fd);
	zero_fd = -1;

	igt_assert(pages);

	ret = mprotect(pages + page_size, page_size, PROT_NONE);
	igt_assert_eq(ret, 0);

	stream_fd = __perf_open(drm_fd, &param, false);

	nanosleep(&(struct timespec){ .tv_sec = 0, .tv_nsec = 5000000 }, NULL);

	/* At this point there should be lots of pending reports to read */

	/* A read that can return at least one record should result in a short
	 * read not an EFAULT if the buffer is smaller than the requested read
	 * size...
	 *
	 * Expect to see a sample record here, but at least skip over any
	 * _RECORD_LOST notifications.
	 */
	do {
		header = (void *)(pages + page_size - record_size);
		ret = read(stream_fd,
			   header,
			   page_size);
		igt_assert(ret > 0);
	} while (header->type == DRM_XE_OA_RECORD_OA_REPORT_LOST);

	igt_assert_eq(ret, record_size);

	/* A read that can't return a single record because it would result
	 * in a fault on buffer overrun should result in an EFAULT error...
	 *
	 * Make sure to weed out all report lost errors before verifying EFAULT.
	 */
	header = (void *)(pages + page_size - 16);
	do {
		ret = read(stream_fd, header, page_size);
	} while (ret > 0 && header->type == DRM_XE_OA_RECORD_OA_REPORT_LOST);
	igt_assert_eq(ret, -1);
	igt_assert_eq(errno, EFAULT);

	/* A read that can't return a single record because the buffer is too
	 * small should result in an ENOSPC error..
	 *
	 * Again, skip over _RECORD_LOST records (smaller than record_size/2)
	 */
	do {
		header = (void *)(pages + page_size - record_size / 2);
		ret = read(stream_fd,
			   header,
			   record_size / 2);
	} while (ret > 0 && header->type == DRM_XE_OA_RECORD_OA_REPORT_LOST);

	igt_assert_eq(ret, -1);
	igt_assert_eq(errno, ENOSPC);

	__perf_close(stream_fd);

	munmap(pages, page_size * 2);
}

static void
test_non_sampling_read_error(void)
{
	uint64_t properties[] = {
		DRM_XE_OA_PROPERTY_OA_UNIT_ID, 0,

		/* XXX: even without periodic sampling we have to
		 * specify at least one sample layout property...
		 */
		DRM_XE_OA_PROPERTY_SAMPLE_OA, true,

		/* OA unit configuration */
		DRM_XE_OA_PROPERTY_OA_METRIC_SET, default_test_set->perf_oa_metrics_set,
		DRM_XE_OA_PROPERTY_OA_FORMAT, __ff(default_test_set->perf_oa_format),

		/* XXX: no sampling exponent */
	};
	struct drm_xe_oa_open_prop param = {
		.num_properties = ARRAY_SIZE(properties) / 2,
		.properties_ptr = to_user_pointer(properties),
	};
	int ret;
	uint8_t buf[1024];

	stream_fd = __perf_open(drm_fd, &param, false);
        set_fd_flags(stream_fd, O_CLOEXEC);

	ret = read(stream_fd, buf, sizeof(buf));
	igt_assert_eq(ret, -1);
	igt_assert_eq(errno, EIO);

	__perf_close(stream_fd);
}

/* Check that attempts to read from a stream while it is disable will return
 * EIO instead of blocking indefinitely.
 */
static void
test_disabled_read_error(void)
{
	int oa_exponent = 5; /* 5 micro seconds */
	uint64_t properties[] = {
		DRM_XE_OA_PROPERTY_OA_UNIT_ID, 0,

		/* XXX: even without periodic sampling we have to
		 * specify at least one sample layout property...
		 */
		DRM_XE_OA_PROPERTY_SAMPLE_OA, true,

		/* OA unit configuration */
		DRM_XE_OA_PROPERTY_OA_METRIC_SET, default_test_set->perf_oa_metrics_set,
		DRM_XE_OA_PROPERTY_OA_FORMAT, __ff(default_test_set->perf_oa_format),
		DRM_XE_OA_PROPERTY_OA_EXPONENT, oa_exponent,
		DRM_XE_OA_PROPERTY_OA_DISABLED, true,
	};
	struct drm_xe_oa_open_prop param = {
		.num_properties = ARRAY_SIZE(properties) / 2,
		.properties_ptr = to_user_pointer(properties),
	};
	uint32_t oa_report0[64];
	uint32_t oa_report1[64];
	uint32_t buf[128] = { 0 };
	int ret;

	stream_fd = __perf_open(drm_fd, &param, false);

	ret = read(stream_fd, buf, sizeof(buf));
	igt_assert_eq(ret, -1);
	igt_assert_eq(errno, EIO);

	__perf_close(stream_fd);

	properties[ARRAY_SIZE(properties) - 1] = false; /* Set DISABLED to false */
	stream_fd = __perf_open(drm_fd, &param, false);
        set_fd_flags(stream_fd, O_CLOEXEC);

	read_2_oa_reports(default_test_set->perf_oa_format,
			  oa_exponent,
			  oa_report0,
			  oa_report1,
			  false); /* not just timer reports */

	do_ioctl(stream_fd, DRM_XE_PERF_IOCTL_DISABLE, 0);

	ret = read(stream_fd, buf, sizeof(buf));
	igt_assert_eq(ret, -1);
	igt_assert_eq(errno, EIO);

	do_ioctl(stream_fd, DRM_XE_PERF_IOCTL_ENABLE, 0);

	read_2_oa_reports(default_test_set->perf_oa_format,
			  oa_exponent,
			  oa_report0,
			  oa_report1,
			  false); /* not just timer reports */

	__perf_close(stream_fd);
}

static void
gen12_test_mi_rpc(const struct intel_execution_engine2 *e2,
		  struct drm_xe_engine_class_instance *hwe)

{
	uint64_t fmt = ((IS_DG2(devid) || IS_METEORLAKE(devid)) &&
			hwe->engine_class == DRM_XE_ENGINE_CLASS_COMPUTE) ?
		XE_OAC_FORMAT_A24u64_B8_C8 : oar_unit_default_format();
	struct intel_perf_metric_set *test_set = metric_set(e2);
	uint64_t properties[] = {
		DRM_XE_OA_PROPERTY_OA_UNIT_ID, 0,

		/* On Gen12, MI RPC uses OAR. OAR is configured only for the
		 * render context that wants to measure the performance. Hence a
		 * context must be specified in the gen12 MI RPC when compared
		 * to previous gens.
		 *
		 * Have a random value here for the context id, but initialize
		 * it once you figure out the context ID for the work to be
		 * measured
		 */
		DRM_XE_OA_PROPERTY_EXEC_QUEUE_ID, UINT64_MAX,

		/* OA unit configuration:
		 * DRM_XE_OA_PROPERTY_SAMPLE_OA is no longer required for Gen12
		 * because the OAR unit increments counters only for the
		 * relevant context. No other parameters are needed since we do
		 * not rely on the OA buffer anymore to normalize the counter
		 * values.
		 */
		DRM_XE_OA_PROPERTY_OA_METRIC_SET, test_set->perf_oa_metrics_set,
		DRM_XE_OA_PROPERTY_OA_FORMAT, __ff(fmt),
		DRM_XE_OA_PROPERTY_OA_ENGINE_INSTANCE, hwe->engine_instance,
	};
	struct drm_xe_oa_open_prop param = {
		.num_properties = has_param_class_instance() ?
				  ARRAY_SIZE(properties) / 2 :
				  (ARRAY_SIZE(properties) / 2) - 2,
		.properties_ptr = to_user_pointer(properties),
	};
	struct buf_ops *bops;
	struct intel_bb *ibb;
	struct intel_buf *buf;
#define INVALID_CTX_ID 0xffffffff
	uint32_t ctx_id = INVALID_CTX_ID;
	uint32_t vm = 0;
	uint32_t *report32;
	size_t format_size_32;
	struct oa_format format = get_oa_format(fmt);

	/* Ensure perf_stream_paranoid is set to 1 by default */
	write_u64_file("/proc/sys/dev/xe/perf_stream_paranoid", 1);

	bops = buf_ops_create(drm_fd);
	if (is_xe_device(drm_fd)) {
		vm = xe_vm_create(drm_fd, 0, 0);
		ctx_id = xe_exec_queue_create(drm_fd, vm, hwe, 0);
	} else {
		ctx_id = gem_context_create(drm_fd);
	}
	igt_assert_neq(ctx_id, INVALID_CTX_ID);
	properties[3] = ctx_id;

	ibb = intel_bb_create_with_context(drm_fd, ctx_id, vm, NULL, BATCH_SZ);
	buf = intel_buf_create(bops, 4096, 1, 8, 64,
			       I915_TILING_NONE, I915_COMPRESSION_NONE);

	buf_map(drm_fd, buf, true);
	memset(buf->ptr, 0x80, 4096);
	intel_buf_unmap(buf);

	stream_fd = __perf_open(drm_fd, &param, false);
        set_fd_flags(stream_fd, O_CLOEXEC);

#define REPORT_ID 0xdeadbeef
#define REPORT_OFFSET 0
	emit_report_perf_count(ibb,
			       buf,
			       REPORT_OFFSET,
			       REPORT_ID);
	intel_bb_flush_render(ibb);
	intel_bb_sync(ibb);

	buf_map(drm_fd, buf, false);
	report32 = buf->ptr;
	format_size_32 = format.size >> 2;
	dump_report(report32, format_size_32, "mi-rpc");

	/* Sanity check reports
	 * reportX_32[0]: report id passed with mi-rpc
	 * reportX_32[1]: timestamp. NOTE: wraps around in ~6 minutes.
	 *
	 * reportX_32[format.b_off]: check if the entire report was filled.
	 * B0 counter falls in the last 64 bytes of this report format.
	 * Since reports are filled in 64 byte blocks, we should be able to
	 * assure that the report was filled by checking the B0 counter. B0
	 * counter is defined to be zero, so we can easily validate it.
	 *
	 * reportX_32[format_size_32]: outside report, make sure only the report
	 * size amount of data was written.
	 */
	igt_assert_eq(report32[0], REPORT_ID);
	igt_assert(oa_timestamp(report32, test_set->perf_oa_format));
	igt_assert_neq(report32[format.b_off >> 2], 0x80808080);
	igt_assert_eq(report32[format_size_32], 0x80808080);

	intel_buf_unmap(buf);
	intel_buf_destroy(buf);
	intel_bb_destroy(ibb);
	if (is_xe_device(drm_fd)) {
		xe_exec_queue_destroy(drm_fd, ctx_id);
		xe_vm_destroy(drm_fd, vm);
	} else {
		gem_context_destroy(drm_fd, ctx_id);
	}
	buf_ops_destroy(bops);
	__perf_close(stream_fd);
}

static void
emit_stall_timestamp_and_rpc(struct intel_bb *ibb,
			     struct intel_buf *dst,
			     int timestamp_offset,
			     int report_dst_offset,
			     uint32_t report_id)
{
	uint32_t pipe_ctl_flags = (PIPE_CONTROL_CS_STALL |
				   PIPE_CONTROL_RENDER_TARGET_FLUSH |
				   PIPE_CONTROL_WRITE_TIMESTAMP);

	intel_bb_add_intel_buf(ibb, dst, true);

	if (intel_gen(devid) >= 8)
		intel_bb_out(ibb, GFX_OP_PIPE_CONTROL(6));
	else
		intel_bb_out(ibb, GFX_OP_PIPE_CONTROL(5));

	intel_bb_out(ibb, pipe_ctl_flags);
	intel_bb_emit_reloc(ibb, dst->handle,
			    I915_GEM_DOMAIN_INSTRUCTION, I915_GEM_DOMAIN_INSTRUCTION,
			    timestamp_offset, dst->addr.offset);
	intel_bb_out(ibb, 0); /* imm lower */
	intel_bb_out(ibb, 0); /* imm upper */

	emit_report_perf_count(ibb, dst, report_dst_offset, report_id);
}

static void gen12_single_ctx_helper(const struct intel_execution_engine2 *e)
{
	struct intel_perf_metric_set *test_set = metric_set(e);
	uint64_t fmt = oar_unit_default_format();
	uint64_t properties[] = {
		DRM_XE_OA_PROPERTY_OA_UNIT_ID, 0,

		/* Have a random value here for the context id, but initialize
		 * it once you figure out the context ID for the work to be
		 * measured
		 */
		DRM_XE_OA_PROPERTY_EXEC_QUEUE_ID, UINT64_MAX,

		/* OA unit configuration:
		 * DRM_XE_OA_PROPERTY_SAMPLE_OA is no longer required for Gen12
		 * because the OAR unit increments counters only for the
		 * relevant context. No other parameters are needed since we do
		 * not rely on the OA buffer anymore to normalize the counter
		 * values.
		 */
		DRM_XE_OA_PROPERTY_OA_METRIC_SET, test_set->perf_oa_metrics_set,
		DRM_XE_OA_PROPERTY_OA_FORMAT, __ff(fmt),
		DRM_XE_OA_PROPERTY_OA_ENGINE_INSTANCE, e->instance,
	};
	struct drm_xe_oa_open_prop param = {
		.num_properties = has_param_class_instance() ?
				  ARRAY_SIZE(properties) / 2 :
				  (ARRAY_SIZE(properties) / 2) - 2,
		.properties_ptr = to_user_pointer(properties),
	};
	struct buf_ops *bops;
	struct intel_bb *ibb0, *ibb1;
	struct intel_buf src[3], dst[3], *dst_buf;
	uint32_t context0_id, context1_id, vm = 0;
	uint32_t *report0_32, *report1_32, *report2_32, *report3_32;
	uint64_t timestamp0_64, timestamp1_64;
	uint64_t delta_ts64, delta_oa32;
	uint64_t delta_ts64_ns, delta_oa32_ns;
	uint64_t delta_delta;
	int width = 800;
	int height = 600;
#define INVALID_CTX_ID 0xffffffff
	uint32_t ctx0_id = INVALID_CTX_ID;
	uint32_t ctx1_id = INVALID_CTX_ID;
	int ret;
	struct accumulator accumulator = {
		.format = fmt
	};

	bops = buf_ops_create(drm_fd);

	for (int i = 0; i < ARRAY_SIZE(src); i++) {
		scratch_buf_init(bops, &src[i], width, height, 0xff0000ff);
		scratch_buf_init(bops, &dst[i], width, height, 0x00ff00ff);
	}

	if (is_xe_device(drm_fd)) {
		vm = xe_vm_create(drm_fd, 0, 0);
		context0_id = xe_exec_queue_create(drm_fd, vm, &xe_engine(drm_fd, 0)->instance, 0);
		context1_id = xe_exec_queue_create(drm_fd, vm, &xe_engine(drm_fd, 0)->instance, 0);
	} else {
		context0_id = gem_context_create(drm_fd);
		context1_id = gem_context_create(drm_fd);
	}
	ibb0 = intel_bb_create_with_context(drm_fd, context0_id, vm, NULL, BATCH_SZ);
	ibb1 = intel_bb_create_with_context(drm_fd, context1_id, vm, NULL, BATCH_SZ);

	igt_debug("submitting warm up render_copy\n");

	/* Submit some early, unmeasured, work to the context we want
	 * to measure to try and catch issues with i915-perf
	 * initializing the HW context ID for filtering.
	 *
	 * We do this because i915-perf single context filtering had
	 * previously only relied on a hook into context pinning to
	 * initialize the HW context ID, instead of also trying to
	 * determine the HW ID while opening the stream, in case it
	 * has already been pinned.
	 *
	 * This wasn't noticed by the previous unit test because we
	 * were opening the stream while the context hadn't been
	 * touched or pinned yet and so it worked out correctly to wait
	 * for the pinning hook.
	 *
	 * Now a buggy version of i915-perf will fail to measure
	 * anything for context0 once this initial render_copy() ends
	 * up pinning the context since there won't ever be a pinning
	 * hook callback.
	 */
	render_copy(ibb0,
		    &src[0], 0, 0, width, height,
		    &dst[0], 0, 0);

	/* Initialize the context parameter to the perf open ioctl here */
	properties[3] = context0_id;

	igt_debug("opening i915-perf stream\n");
	stream_fd = __perf_open(drm_fd, &param, false);
        set_fd_flags(stream_fd, O_CLOEXEC);

	dst_buf = intel_buf_create(bops, 4096, 1, 8, 64,
				   I915_TILING_NONE,
				   I915_COMPRESSION_NONE);

	/* Set write domain to cpu briefly to fill the buffer with 80s */
	buf_map(drm_fd, dst_buf, true /* write enable */);
	memset(dst_buf->ptr, 0x80, 2048);
	memset((uint8_t *) dst_buf->ptr + 2048, 0, 2048);
	intel_buf_unmap(dst_buf);

	/* Submit an mi-rpc to context0 before measurable work */
#define BO_TIMESTAMP_OFFSET0 1024
#define BO_REPORT_OFFSET0 0
#define BO_REPORT_ID0 0xdeadbeef
	emit_stall_timestamp_and_rpc(ibb0,
				     dst_buf,
				     BO_TIMESTAMP_OFFSET0,
				     BO_REPORT_OFFSET0,
				     BO_REPORT_ID0);
	intel_bb_flush_render(ibb0);

	/* Remove intel_buf from ibb0 added implicitly in rendercopy */
	intel_bb_remove_intel_buf(ibb0, dst_buf);

	/* This is the work/context that is measured for counter increments */
	render_copy(ibb0,
		    &src[0], 0, 0, width, height,
		    &dst[0], 0, 0);
	intel_bb_flush_render(ibb0);

	/* Submit an mi-rpc to context1 before work
	 *
	 * On gen12, this measurement should just yield counters that are
	 * all zeroes, since the counters will only increment for the
	 * context passed to perf open ioctl
	 */
#define BO_TIMESTAMP_OFFSET2 1040
#define BO_REPORT_OFFSET2 512
#define BO_REPORT_ID2 0x00c0ffee
	emit_stall_timestamp_and_rpc(ibb1,
				     dst_buf,
				     BO_TIMESTAMP_OFFSET2,
				     BO_REPORT_OFFSET2,
				     BO_REPORT_ID2);
	intel_bb_flush_render(ibb1);

	/* Submit two copies on the other context to avoid a false
	 * positive in case the driver somehow ended up filtering for
	 * context1
	 */
	render_copy(ibb1,
		    &src[1], 0, 0, width, height,
		    &dst[1], 0, 0);

	render_copy(ibb1,
		    &src[2], 0, 0, width, height,
		    &dst[2], 0, 0);
	intel_bb_flush_render(ibb1);

	/* Submit an mi-rpc to context1 after all work */
#define BO_TIMESTAMP_OFFSET3 1048
#define BO_REPORT_OFFSET3 768
#define BO_REPORT_ID3 0x01c0ffee
	emit_stall_timestamp_and_rpc(ibb1,
				     dst_buf,
				     BO_TIMESTAMP_OFFSET3,
				     BO_REPORT_OFFSET3,
				     BO_REPORT_ID3);
	intel_bb_flush_render(ibb1);

	/* Remove intel_buf from ibb1 added implicitly in rendercopy */
	intel_bb_remove_intel_buf(ibb1, dst_buf);

	/* Submit an mi-rpc to context0 after all measurable work */
#define BO_TIMESTAMP_OFFSET1 1032
#define BO_REPORT_OFFSET1 256
#define BO_REPORT_ID1 0xbeefbeef
	emit_stall_timestamp_and_rpc(ibb0,
				     dst_buf,
				     BO_TIMESTAMP_OFFSET1,
				     BO_REPORT_OFFSET1,
				     BO_REPORT_ID1);
	intel_bb_flush_render(ibb0);
	intel_bb_sync(ibb0);
	intel_bb_sync(ibb1);

	buf_map(drm_fd, dst_buf, false);

	/* Sanity check reports
	 * reportX_32[0]: report id passed with mi-rpc
	 * reportX_32[1]: timestamp
	 * reportX_32[2]: context id
	 *
	 * report0_32: start of measurable work
	 * report1_32: end of measurable work
	 * report2_32: start of other work
	 * report3_32: end of other work
	 */
	report0_32 = dst_buf->ptr;
	igt_assert_eq(report0_32[0], 0xdeadbeef);
	igt_assert(oa_timestamp(report0_32, fmt));
	ctx0_id = report0_32[2];
	igt_debug("MI_RPC(start) CTX ID: %u\n", ctx0_id);
	dump_report(report0_32, 64, "report0_32");

	report1_32 = report0_32 + 64;
	igt_assert_eq(report1_32[0], 0xbeefbeef);
	igt_assert(oa_timestamp(report1_32, fmt));
	ctx1_id = report1_32[2];
	igt_debug("CTX ID1: %u\n", ctx1_id);
	dump_report(report1_32, 64, "report1_32");

	/* Verify that counters in context1 are all zeroes */
	report2_32 = report0_32 + 128;
	igt_assert_eq(report2_32[0], 0x00c0ffee);
	igt_assert(oa_timestamp(report2_32, fmt));
	dump_report(report2_32, 64, "report2_32");
	igt_assert_eq(0, memcmp(&report2_32[4],
				(uint8_t *) dst_buf->ptr + 2048,
				240));

	report3_32 = report0_32 + 192;
	igt_assert_eq(report3_32[0], 0x01c0ffee);
	igt_assert(oa_timestamp(report3_32, fmt));
	dump_report(report3_32, 64, "report3_32");
	igt_assert_eq(0, memcmp(&report3_32[4],
				(uint8_t *) dst_buf->ptr + 2048,
				240));

	/* Accumulate deltas for counters - A0, A21 and A26 */
	memset(accumulator.deltas, 0, sizeof(accumulator.deltas));
	accumulate_reports(&accumulator, report0_32, report1_32);
	igt_debug("total: A0 = %"PRIu64", A21 = %"PRIu64", A26 = %"PRIu64"\n",
			accumulator.deltas[2 + 0],
			accumulator.deltas[2 + 21],
			accumulator.deltas[2 + 26]);

	igt_debug("oa_timestamp32 0 = %"PRIu64"\n", oa_timestamp(report0_32, fmt));
	igt_debug("oa_timestamp32 1 = %"PRIu64"\n", oa_timestamp(report1_32, fmt));
	igt_debug("ctx_id 0 = %u\n", report0_32[2]);
	igt_debug("ctx_id 1 = %u\n", report1_32[2]);

	/* The delta as calculated via the PIPE_CONTROL timestamp or
	 * the OA report timestamps should be almost identical but
	 * allow a 500 nanoseconds margin.
	 */
	timestamp0_64 = *(uint64_t *)(((uint8_t *)dst_buf->ptr) + BO_TIMESTAMP_OFFSET0);
	timestamp1_64 = *(uint64_t *)(((uint8_t *)dst_buf->ptr) + BO_TIMESTAMP_OFFSET1);

	igt_debug("ts_timestamp64 0 = %"PRIu64"\n", timestamp0_64);
	igt_debug("ts_timestamp64 1 = %"PRIu64"\n", timestamp1_64);

	delta_ts64 = timestamp1_64 - timestamp0_64;
	delta_oa32 = oa_timestamp_delta(report1_32, report0_32, fmt);

	/* Sanity check that we can pass the delta to timebase_scale */
	delta_oa32_ns = timebase_scale(delta_oa32);
	delta_ts64_ns = cs_timebase_scale(delta_ts64);

	igt_debug("oa32 delta = %"PRIu64", = %"PRIu64"ns\n",
			delta_oa32, delta_oa32_ns);
	igt_debug("ts64 delta = %"PRIu64", = %"PRIu64"ns\n",
			delta_ts64, delta_ts64_ns);

	delta_delta = delta_ts64_ns > delta_oa32_ns ?
		      (delta_ts64_ns - delta_oa32_ns) :
		      (delta_oa32_ns - delta_ts64_ns);
	if (delta_delta > 500) {
		igt_debug("delta_delta = %"PRIu64". exceeds margin, skipping..\n",
			  delta_delta);
		exit(EAGAIN);
	}

	igt_debug("n samples written = %"PRIu64"/%"PRIu64" (%ix%i)\n",
		  accumulator.deltas[2 + 21],
		  accumulator.deltas[2 + 26],
		  width, height);
	accumulator_print(&accumulator, "filtered");

	/* Verify that the work actually happened by comparing the src
	 * and dst buffers
	 */
	buf_map(drm_fd, &src[0], false);
	buf_map(drm_fd, &dst[0], false);

	ret = memcmp(src[0].ptr, dst[0].ptr, 4 * width * height);
	intel_buf_unmap(&src[0]);
	intel_buf_unmap(&dst[0]);

	if (ret != 0) {
		accumulator_print(&accumulator, "total");
		exit(EAGAIN);
	}

	/* FIXME: can we deduce the presence of A26 from get_oa_format(fmt)? */
	if (IS_LUNARLAKE(devid))
		goto skip_check;

	/* Check that this test passed. The test measures the number of 2x2
	 * samples written to the render target using the counter A26. For
	 * OAR, this counter will only have increments relevant to this specific
	 * context. The value equals the width * height of the rendered work.
	 */
	igt_assert_eq(accumulator.deltas[2 + 26], width * height);

 skip_check:
	/* Clean up */
	for (int i = 0; i < ARRAY_SIZE(src); i++) {
		intel_buf_close(bops, &src[i]);
		intel_buf_close(bops, &dst[i]);
	}

	intel_buf_unmap(dst_buf);
	intel_buf_destroy(dst_buf);
	intel_bb_destroy(ibb0);
	intel_bb_destroy(ibb1);
	if (is_xe_device(drm_fd)) {
		xe_exec_queue_destroy(drm_fd, context0_id);
		xe_exec_queue_destroy(drm_fd, context1_id);
		xe_vm_destroy(drm_fd, vm);
	} else {
		gem_context_destroy(drm_fd, context0_id);
		gem_context_destroy(drm_fd, context1_id);
	}
	buf_ops_destroy(bops);
	__perf_close(stream_fd);
}

static void gen12_single_ctx_helper_one_ctx(const struct intel_execution_engine2 *e)
{
	struct intel_perf_metric_set *test_set = metric_set(e);
	uint64_t fmt = oar_unit_default_format();
	uint64_t properties[] = {
		DRM_XE_OA_PROPERTY_OA_UNIT_ID, 0,

		/* Have a random value here for the context id, but initialize
		 * it once you figure out the context ID for the work to be
		 * measured
		 */
		DRM_XE_OA_PROPERTY_EXEC_QUEUE_ID, UINT64_MAX,

		/* OA unit configuration:
		 * DRM_XE_OA_PROPERTY_SAMPLE_OA is no longer required for Gen12
		 * because the OAR unit increments counters only for the
		 * relevant context. No other parameters are needed since we do
		 * not rely on the OA buffer anymore to normalize the counter
		 * values.
		 */
		DRM_XE_OA_PROPERTY_OA_METRIC_SET, test_set->perf_oa_metrics_set,
		DRM_XE_OA_PROPERTY_OA_FORMAT, __ff(fmt),
		DRM_XE_OA_PROPERTY_OA_ENGINE_INSTANCE, e->instance,
	};
	struct drm_xe_oa_open_prop param = {
		.num_properties = has_param_class_instance() ?
				  ARRAY_SIZE(properties) / 2 :
				  (ARRAY_SIZE(properties) / 2) - 2,
		.properties_ptr = to_user_pointer(properties),
	};
	struct buf_ops *bops;
	struct intel_bb *ibb0;
	struct intel_buf src[3], dst[3], *dst_buf;
	uint32_t context0_id, vm = 0;
	uint32_t *report0_32, *report1_32;
	uint64_t timestamp0_64, timestamp1_64;
	uint64_t delta_ts64, delta_oa32;
	uint64_t delta_ts64_ns, delta_oa32_ns;
	uint64_t delta_delta;
	int width = 800;
	int height = 600;
#define INVALID_CTX_ID 0xffffffff
	uint32_t ctx0_id = INVALID_CTX_ID;
	uint32_t ctx1_id = INVALID_CTX_ID;
	int ret;
	struct accumulator accumulator = {
		.format = fmt
	};

	bops = buf_ops_create(drm_fd);

	for (int i = 0; i < ARRAY_SIZE(src); i++) {
		scratch_buf_init(bops, &src[i], width, height, 0xff0000ff);
		scratch_buf_init(bops, &dst[i], width, height, 0x00ff00ff);
	}

	if (is_xe_device(drm_fd)) {
		vm = xe_vm_create(drm_fd, 0, 0);
		context0_id = xe_exec_queue_create(drm_fd, vm, &xe_engine(drm_fd, 0)->instance, 0);
	} else {
		context0_id = gem_context_create(drm_fd);
	}
	ibb0 = intel_bb_create_with_context(drm_fd, context0_id, vm, NULL, BATCH_SZ);

	igt_debug("submitting warm up render_copy\n");

	/* Submit some early, unmeasured, work to the context we want
	 * to measure to try and catch issues with i915-perf
	 * initializing the HW context ID for filtering.
	 *
	 * We do this because i915-perf single context filtering had
	 * previously only relied on a hook into context pinning to
	 * initialize the HW context ID, instead of also trying to
	 * determine the HW ID while opening the stream, in case it
	 * has already been pinned.
	 *
	 * This wasn't noticed by the previous unit test because we
	 * were opening the stream while the context hadn't been
	 * touched or pinned yet and so it worked out correctly to wait
	 * for the pinning hook.
	 *
	 * Now a buggy version of i915-perf will fail to measure
	 * anything for context0 once this initial render_copy() ends
	 * up pinning the context since there won't ever be a pinning
	 * hook callback.
	 */
	render_copy(ibb0,
		    &src[0], 0, 0, width, height,
		    &dst[0], 0, 0);

	/* Initialize the context parameter to the perf open ioctl here */
	properties[3] = context0_id;

	igt_debug("opening i915-perf stream\n");
	stream_fd = __perf_open(drm_fd, &param, false);

	dst_buf = intel_buf_create(bops, 4096, 1, 8, 64,
				   I915_TILING_NONE,
				   I915_COMPRESSION_NONE);

	/* Set write domain to cpu briefly to fill the buffer with 80s */
	buf_map(drm_fd, dst_buf, true /* write enable */);
	memset(dst_buf->ptr, 0x80, 2048);
	memset((uint8_t *) dst_buf->ptr + 2048, 0, 2048);
	intel_buf_unmap(dst_buf);

	/* Submit an mi-rpc to context0 before measurable work */
#define BO_TIMESTAMP_OFFSET0 1024
#define BO_REPORT_OFFSET0 0
#define BO_REPORT_ID0 0xdeadbeef
	emit_stall_timestamp_and_rpc(ibb0,
				     dst_buf,
				     BO_TIMESTAMP_OFFSET0,
				     BO_REPORT_OFFSET0,
				     BO_REPORT_ID0);
	intel_bb_flush_render(ibb0);

	/* Remove intel_buf from ibb0 added implicitly in rendercopy */
	intel_bb_remove_intel_buf(ibb0, dst_buf);

	/* This is the work/context that is measured for counter increments */
	render_copy(ibb0,
		    &src[0], 0, 0, width, height,
		    &dst[0], 0, 0);
	intel_bb_flush_render(ibb0);

	/* Submit an mi-rpc to context0 after all measurable work */
#define BO_TIMESTAMP_OFFSET1 1032
#define BO_REPORT_OFFSET1 256
#define BO_REPORT_ID1 0xbeefbeef
	emit_stall_timestamp_and_rpc(ibb0,
				     dst_buf,
				     BO_TIMESTAMP_OFFSET1,
				     BO_REPORT_OFFSET1,
				     BO_REPORT_ID1);
	intel_bb_flush_render(ibb0);
	intel_bb_sync(ibb0);

	buf_map(drm_fd, dst_buf, false);

	/* Sanity check reports
	 * reportX_32[0]: report id passed with mi-rpc
	 * reportX_32[1]: timestamp
	 * reportX_32[2]: context id
	 *
	 * report0_32: start of measurable work
	 * report1_32: end of measurable work
	 * report2_32: start of other work
	 * report3_32: end of other work
	 */
	report0_32 = dst_buf->ptr;
	igt_assert_eq(report0_32[0], 0xdeadbeef);
	igt_assert(oa_timestamp(report0_32, fmt));
	ctx0_id = report0_32[2];
	igt_debug("MI_RPC(start) CTX ID: %u\n", ctx0_id);
	dump_report(report0_32, 64, "report0_32");

	report1_32 = report0_32 + 64;
	igt_assert_eq(report1_32[0], 0xbeefbeef);
	igt_assert(oa_timestamp(report1_32, fmt));
	ctx1_id = report1_32[2];
	igt_debug("CTX ID1: %u\n", ctx1_id);
	dump_report(report1_32, 64, "report1_32");

	/* Accumulate deltas for counters - A0, A21 and A26 */
	memset(accumulator.deltas, 0, sizeof(accumulator.deltas));
	accumulate_reports(&accumulator, report0_32, report1_32);
	igt_debug("total: A0 = %"PRIu64", A21 = %"PRIu64", A26 = %"PRIu64"\n",
			accumulator.deltas[2 + 0],
			accumulator.deltas[2 + 21],
			accumulator.deltas[2 + 26]);

	igt_debug("oa_timestamp32 0 = %"PRIu64"\n", oa_timestamp(report0_32, fmt));
	igt_debug("oa_timestamp32 1 = %"PRIu64"\n", oa_timestamp(report1_32, fmt));
	igt_debug("ctx_id 0 = %u\n", report0_32[2]);
	igt_debug("ctx_id 1 = %u\n", report1_32[2]);

	/* The delta as calculated via the PIPE_CONTROL timestamp or
	 * the OA report timestamps should be almost identical but
	 * allow a 500 nanoseconds margin.
	 */
	timestamp0_64 = *(uint64_t *)(((uint8_t *)dst_buf->ptr) + BO_TIMESTAMP_OFFSET0);
	timestamp1_64 = *(uint64_t *)(((uint8_t *)dst_buf->ptr) + BO_TIMESTAMP_OFFSET1);

	igt_debug("ts_timestamp64 0 = %"PRIu64"\n", timestamp0_64);
	igt_debug("ts_timestamp64 1 = %"PRIu64"\n", timestamp1_64);

	delta_ts64 = timestamp1_64 - timestamp0_64;
	delta_oa32 = oa_timestamp_delta(report1_32, report0_32, fmt);

	/* Sanity check that we can pass the delta to timebase_scale */
	delta_oa32_ns = timebase_scale(delta_oa32);
	delta_ts64_ns = cs_timebase_scale(delta_ts64);

	igt_debug("oa32 delta = %"PRIu64", = %"PRIu64"ns\n",
			delta_oa32, delta_oa32_ns);
	igt_debug("ts64 delta = %"PRIu64", = %"PRIu64"ns\n",
			delta_ts64, delta_ts64_ns);

	delta_delta = delta_ts64_ns > delta_oa32_ns ?
		      (delta_ts64_ns - delta_oa32_ns) :
		      (delta_oa32_ns - delta_ts64_ns);
	if (delta_delta > 500) {
		igt_debug("delta_delta = %"PRIu64". exceeds margin, skipping..\n",
			  delta_delta);
		exit(EAGAIN);
	}

	igt_debug("n samples written = %"PRIu64"/%"PRIu64" (%ix%i)\n",
		  accumulator.deltas[2 + 21],
		  accumulator.deltas[2 + 26],
		  width, height);
	accumulator_print(&accumulator, "filtered");

	/* Verify that the work actually happened by comparing the src
	 * and dst buffers
	 */
	buf_map(drm_fd, &src[0], false);
	buf_map(drm_fd, &dst[0], false);

	ret = memcmp(src[0].ptr, dst[0].ptr, 4 * width * height);
	intel_buf_unmap(&src[0]);
	intel_buf_unmap(&dst[0]);

	if (ret != 0) {
		accumulator_print(&accumulator, "total");
		exit(EAGAIN);
	}

	/* Check that this test passed. The test measures the number of 2x2
	 * samples written to the render target using the counter A26. For
	 * OAR, this counter will only have increments relevant to this specific
	 * context. The value equals the width * height of the rendered work.
	 */
	igt_assert_eq(accumulator.deltas[2 + 26], width * height);

	/* Clean up */
	for (int i = 0; i < ARRAY_SIZE(src); i++) {
		intel_buf_close(bops, &src[i]);
		intel_buf_close(bops, &dst[i]);
	}

	intel_buf_unmap(dst_buf);
	intel_buf_destroy(dst_buf);
	intel_bb_destroy(ibb0);
	if (is_xe_device(drm_fd)) {
		xe_exec_queue_destroy(drm_fd, context0_id);
		xe_vm_destroy(drm_fd, vm);
	} else {
		gem_context_destroy(drm_fd, context0_id);
	}
	buf_ops_destroy(bops);
	__perf_close(stream_fd);
}

static void
gen12_test_single_ctx_render_target_writes_a_counter(const struct intel_execution_engine2 *e,
						     bool one_ctx)
{
	int child_ret;
	struct igt_helper_process child = {};

	/* Ensure perf_stream_paranoid is set to 1 by default */
	write_u64_file("/proc/sys/dev/xe/perf_stream_paranoid", 1);

	do {
		igt_fork_helper(&child) {
			/* A local device for local resources. */
			drm_fd = drm_reopen_driver(drm_fd);

			igt_drop_root();

			if (one_ctx)
				gen12_single_ctx_helper_one_ctx(e);
			else
				gen12_single_ctx_helper(e);

			drm_close_driver(drm_fd);
		}
		child_ret = igt_wait_helper(&child);
		igt_assert(WEXITSTATUS(child_ret) == EAGAIN ||
			   WEXITSTATUS(child_ret) == 0);
	} while (WEXITSTATUS(child_ret) == EAGAIN);
}

static unsigned long rc6_residency_ms(void)
{
	return sysfs_read(RC6_RESIDENCY_MS);
}

static void
test_rc6_disable(void)
{
	uint64_t properties[] = {
		DRM_XE_OA_PROPERTY_OA_UNIT_ID, 0,

		/* Include OA reports in samples */
		DRM_XE_OA_PROPERTY_SAMPLE_OA, true,

		/* OA unit configuration */
		DRM_XE_OA_PROPERTY_OA_METRIC_SET, default_test_set->perf_oa_metrics_set,
		DRM_XE_OA_PROPERTY_OA_FORMAT, __ff(default_test_set->perf_oa_format),
		DRM_XE_OA_PROPERTY_OA_EXPONENT, oa_exp_1_millisec,
	};
	struct drm_xe_oa_open_prop param = {
		.num_properties = ARRAY_SIZE(properties) / 2,
		.properties_ptr = to_user_pointer(properties),
	};
	unsigned long rc6_start, rc6_end, rc6_enabled;

	rc6_enabled = 0;
	igt_sysfs_rps_scanf(sysfs, RC6_ENABLE, "%lu", &rc6_enabled);
	igt_require(rc6_enabled);

	/* Verify rc6 is functional by measuring residency while idle */
	gem_quiescent_gpu(drm_fd);
	rc6_start = rc6_residency_ms();
	usleep(50000);
	rc6_end = rc6_residency_ms();
	igt_require(rc6_end != rc6_start);

	/* While OA is active, we keep rc6 disabled so we don't lose metrics */
	stream_fd = __perf_open(drm_fd, &param, false);

	rc6_start = rc6_residency_ms();
	usleep(50000);
	rc6_end = rc6_residency_ms();
	igt_assert_eq(rc6_end - rc6_start, 0);

	__perf_close(stream_fd);
	gem_quiescent_gpu(drm_fd);

	/* But once OA is closed, we expect the device to sleep again */
	rc6_start = rc6_residency_ms();
	usleep(50000);
	rc6_end = rc6_residency_ms();
	igt_assert_neq(rc6_end - rc6_start, 0);
}

static void
test_stress_open_close(const struct intel_execution_engine2 *e)
{
	struct intel_perf_metric_set *test_set = metric_set(e);

	load_helper_init();
	load_helper_run(HIGH);

	igt_until_timeout(2) {
		int oa_exponent = 5; /* 5 micro seconds */
		uint64_t properties[] = {
			DRM_XE_OA_PROPERTY_OA_UNIT_ID, 0,

			/* XXX: even without periodic sampling we have to
			 * specify at least one sample layout property...
			 */
			DRM_XE_OA_PROPERTY_SAMPLE_OA, true,

			/* OA unit configuration */
			DRM_XE_OA_PROPERTY_OA_METRIC_SET, test_set->perf_oa_metrics_set,
			DRM_XE_OA_PROPERTY_OA_FORMAT, __ff(test_set->perf_oa_format),
			DRM_XE_OA_PROPERTY_OA_EXPONENT, oa_exponent,
			DRM_XE_OA_PROPERTY_OA_DISABLED, true,
			DRM_XE_OA_PROPERTY_OA_ENGINE_INSTANCE, e->instance,
		};
		struct drm_xe_oa_open_prop param = {
			.num_properties = has_param_class_instance() ?
					  ARRAY_SIZE(properties) / 2 :
					  (ARRAY_SIZE(properties) / 2) - 2,
			.properties_ptr = to_user_pointer(properties),
		};

		stream_fd = __perf_open(drm_fd, &param, false);
		__perf_close(stream_fd);
	}

	load_helper_stop();
	load_helper_fini();
}

static int __i915_perf_add_config(int fd, struct drm_xe_oa_config *config)
{
	int ret = xe_perf_ioctl(fd, DRM_IOCTL_XE_PERF, DRM_XE_PERF_OP_ADD_CONFIG, config);
	if (ret < 0)
		ret = -errno;
	return ret;
}

static int i915_perf_add_config(int fd, struct drm_xe_oa_config *config)
{
	int config_id = __i915_perf_add_config(fd, config);

	igt_debug("config_id=%i\n", config_id);
	igt_assert(config_id > 0);

	return config_id;
}

static void i915_perf_remove_config(int fd, uint64_t config_id)
{
	igt_assert_eq(xe_perf_ioctl(fd, DRM_IOCTL_XE_PERF, DRM_XE_PERF_OP_REMOVE_CONFIG,
				    &config_id), 0);
}

static bool has_i915_perf_userspace_config(int fd)
{
	uint64_t config = 0;
	int ret = xe_perf_ioctl(fd, DRM_IOCTL_XE_PERF, DRM_XE_PERF_OP_REMOVE_CONFIG, &config);
	igt_assert_eq(ret, -1);

	igt_debug("errno=%i\n", errno);

	return errno != EINVAL;
}

#define SAMPLE_MUX_REG (IS_LUNARLAKE(xe_dev_id(drm_fd)) ? \
			0x13000 /* PES* */ : 0x9888 /* NOA_WRITE */)

static void
test_invalid_create_userspace_config(void)
{
	struct drm_xe_oa_config config;
	const char *uuid = "01234567-0123-0123-0123-0123456789ab";
	const char *invalid_uuid = "blablabla-wrong";
	uint32_t mux_regs[] = { SAMPLE_MUX_REG, 0x0 };
	uint32_t invalid_mux_regs[] = { 0x12345678 /* invalid register */, 0x0 };

	igt_require(has_i915_perf_userspace_config(drm_fd));

	memset(&config, 0, sizeof(config));

	/* invalid uuid */
	strncpy(config.uuid, invalid_uuid, sizeof(config.uuid));
	config.n_regs = 1;
	config.regs_ptr = to_user_pointer(mux_regs);

	igt_assert_eq(__i915_perf_add_config(drm_fd, &config), -EINVAL);

	/* invalid mux_regs */
	memcpy(config.uuid, uuid, sizeof(config.uuid));
	config.n_regs = 1;
	config.regs_ptr = to_user_pointer(invalid_mux_regs);

	igt_assert_eq(__i915_perf_add_config(drm_fd, &config), -EINVAL);

	/* empty config */
	memcpy(config.uuid, uuid, sizeof(config.uuid));
	config.n_regs = 0;
	config.regs_ptr = to_user_pointer(mux_regs);

	igt_assert_eq(__i915_perf_add_config(drm_fd, &config), -EINVAL);

	/* empty config with null pointer */
	memcpy(config.uuid, uuid, sizeof(config.uuid));
	config.n_regs = 1;
	config.regs_ptr = to_user_pointer(NULL);

	igt_assert_eq(__i915_perf_add_config(drm_fd, &config), -EINVAL);

	/* invalid pointer */
	memcpy(config.uuid, uuid, sizeof(config.uuid));
	config.n_regs = 42;
	config.regs_ptr = to_user_pointer((void *) 0xDEADBEEF);

	igt_assert_eq(__i915_perf_add_config(drm_fd, &config), -EFAULT);
}

static void
test_invalid_remove_userspace_config(void)
{
	struct drm_xe_oa_config config;
	const char *uuid = "01234567-0123-0123-0123-0123456789ab";
	uint32_t mux_regs[] = { SAMPLE_MUX_REG, 0x0 };
	uint64_t config_id, wrong_config_id = 999999999;
	char path[512];

	igt_require(has_i915_perf_userspace_config(drm_fd));

	snprintf(path, sizeof(path), "metrics/%s/id", uuid);

	/* Destroy previous configuration if present */
	if (try_sysfs_read_u64(path, &config_id))
		i915_perf_remove_config(drm_fd, config_id);

	memset(&config, 0, sizeof(config));

	memcpy(config.uuid, uuid, sizeof(config.uuid));

	config.n_regs = 1;
	config.regs_ptr = to_user_pointer(mux_regs);

	config_id = i915_perf_add_config(drm_fd, &config);

	/* Removing configs without permissions should fail. */
	igt_fork(child, 1) {
		igt_drop_root();

		xe_perf_ioctl_err(drm_fd, DRM_IOCTL_XE_PERF, DRM_XE_PERF_OP_REMOVE_CONFIG,
				  &config_id, EACCES);
	}
	igt_waitchildren();

	/* Removing invalid config ID should fail. */
	xe_perf_ioctl_err(drm_fd, DRM_IOCTL_XE_PERF, DRM_XE_PERF_OP_REMOVE_CONFIG,
			  &wrong_config_id, ENOENT);

	i915_perf_remove_config(drm_fd, config_id);
}

static void
test_create_destroy_userspace_config(void)
{
	struct drm_xe_oa_config config;
	const char *uuid = "01234567-0123-0123-0123-0123456789ab";
	uint32_t mux_regs[] = { SAMPLE_MUX_REG, 0x0 };
	uint32_t regs[100];
	int i;
	uint64_t config_id;
	uint64_t properties[] = {
		DRM_XE_OA_PROPERTY_OA_UNIT_ID, 0,

		DRM_XE_OA_PROPERTY_OA_METRIC_SET, 0, /* Filled later */

		/* OA unit configuration */
		DRM_XE_OA_PROPERTY_SAMPLE_OA, true,
		DRM_XE_OA_PROPERTY_OA_FORMAT, __ff(default_test_set->perf_oa_format),
		DRM_XE_OA_PROPERTY_OA_EXPONENT, oa_exp_1_millisec,
		DRM_XE_OA_PROPERTY_OA_DISABLED, true,
		DRM_XE_OA_PROPERTY_OA_METRIC_SET
	};
	struct drm_xe_oa_open_prop param = {
		.num_properties = ARRAY_SIZE(properties) / 2,
		.properties_ptr = to_user_pointer(properties),
	};
	char path[512];

	igt_require(has_i915_perf_userspace_config(drm_fd));

	snprintf(path, sizeof(path), "metrics/%s/id", uuid);

	/* Destroy previous configuration if present */
	if (try_sysfs_read_u64(path, &config_id))
		i915_perf_remove_config(drm_fd, config_id);

	memset(&config, 0, sizeof(config));
	memcpy(config.uuid, uuid, sizeof(config.uuid));

	regs[0] = mux_regs[0];
	regs[1] = mux_regs[1];
	/* Flex EU counters */
	for (i = 1; i < ARRAY_SIZE(regs) / 2; i++) {
		regs[i * 2] = 0xe458; /* EU_PERF_CNTL0 */
		regs[i * 2 + 1] = 0x0;
	}
	config.regs_ptr = to_user_pointer(regs);
	config.n_regs = ARRAY_SIZE(regs) / 2;

	/* Creating configs without permissions shouldn't work. */
	igt_fork(child, 1) {
		igt_drop_root();

		igt_assert_eq(__i915_perf_add_config(drm_fd, &config), -EACCES);
	}
	igt_waitchildren();

	/* Create a new config */
	config_id = i915_perf_add_config(drm_fd, &config);

	/* Verify that adding the another config with the same uuid fails. */
	igt_assert_eq(__i915_perf_add_config(drm_fd, &config), -EADDRINUSE);

	/* Try to use the new config */
	properties[3] = config_id;
	stream_fd = __perf_open(drm_fd, &param, false);

	/* Verify that destroying the config doesn't yield any error. */
	i915_perf_remove_config(drm_fd, config_id);

	/* Read the config to verify shouldn't raise any issue. */
	config_id = i915_perf_add_config(drm_fd, &config);

	__perf_close(stream_fd);

	i915_perf_remove_config(drm_fd, config_id);
}

/* Registers required by userspace. This list should be maintained by
 * the OA configs developers and agreed upon with kernel developers as
 * some of the registers have bits used by the kernel (for workarounds
 * for instance) and other bits that need to be set by the OA configs.
 */
static void
test_whitelisted_registers_userspace_config(void)
{
	struct drm_xe_oa_config config;
	const char *uuid = "01234567-0123-0123-0123-0123456789ab";
	uint32_t regs[600];
	uint32_t i;
	uint32_t oa_start_trig1, oa_start_trig8;
	uint32_t oa_report_trig1, oa_report_trig8;
	uint64_t config_id;
	char path[512];
	int ret;
	const uint32_t flex[] = {
		0xe458,
		0xe558,
		0xe658,
		0xe758,
		0xe45c,
		0xe55c,
		0xe65c
	};

	igt_require(has_i915_perf_userspace_config(drm_fd));

	snprintf(path, sizeof(path), "metrics/%s/id", uuid);

	if (try_sysfs_read_u64(path, &config_id))
		i915_perf_remove_config(drm_fd, config_id);

	memset(&config, 0, sizeof(config));
	memcpy(config.uuid, uuid, sizeof(config.uuid));

	if (intel_gen(devid) >= 12) {
		oa_start_trig1 = 0xd900;
		oa_start_trig8 = 0xd91c;
		oa_report_trig1 = 0xd920;
		oa_report_trig8 = 0xd93c;
	} else {
		oa_start_trig1 = 0x2710;
		oa_start_trig8 = 0x272c;
		oa_report_trig1 = 0x2740;
		oa_report_trig8 = 0x275c;
	}

	/* b_counters_regs: OASTARTTRIG[1-8] */
	for (i = oa_start_trig1; i <= oa_start_trig8; i += 4) {
		regs[config.n_regs * 2] = i;
		regs[config.n_regs * 2 + 1] = 0;
		config.n_regs++;
	}
	/* b_counters_regs: OAREPORTTRIG[1-8] */
	for (i = oa_report_trig1; i <= oa_report_trig8; i += 4) {
		regs[config.n_regs * 2] = i;
		regs[config.n_regs * 2 + 1] = 0;
		config.n_regs++;
	}

	/* Flex EU registers, only from Gen8+. */
	for (i = 0; i < ARRAY_SIZE(flex); i++) {
		regs[config.n_regs * 2] = flex[i];
		regs[config.n_regs * 2 + 1] = 0;
		config.n_regs++;
	}

	/* Mux registers (too many of them, just checking bounds) */
	/* NOA_WRITE */
	regs[config.n_regs * 2] = SAMPLE_MUX_REG;
	regs[config.n_regs * 2 + 1] = 0;
	config.n_regs++;

	/* NOA_CONFIG */
	if (!IS_LUNARLAKE(devid)) {
		regs[config.n_regs * 2] = 0xD04;
		regs[config.n_regs * 2 + 1] = 0;
		config.n_regs++;
		regs[config.n_regs * 2] = 0xD2C;
		regs[config.n_regs * 2 + 1] = 0;
		config.n_regs++;
	}
	if (!IS_LUNARLAKE(devid) && !IS_METEORLAKE(devid)) {
		/* WAIT_FOR_RC6_EXIT */
		regs[config.n_regs * 2] = 0x20CC;
		regs[config.n_regs * 2 + 1] = 0;
		config.n_regs++;
	}

	config.regs_ptr = (uintptr_t) regs;

	/* Create a new config */
	ret = xe_perf_ioctl(drm_fd, DRM_IOCTL_XE_PERF, DRM_XE_PERF_OP_ADD_CONFIG, &config);
	igt_assert(ret > 0); /* Config 0 should be used by the kernel */
	config_id = ret;

	i915_perf_remove_config(drm_fd, config_id);
}

#define GEN12_OAG_OASTATUS (0xdafc)
#define GEN12_OAG_MMIOTRIGGER (0xdb1c)

static const uint32_t oa_wl[] = {
	GEN12_OAG_MMIOTRIGGER,
	GEN12_OAG_OASTATUS,
};

static const uint32_t nonpriv_slot_offsets[] = {
	0x4d0, 0x4d4, 0x4d8, 0x4dc, 0x4e0, 0x4e4, 0x4e8, 0x4ec,
	0x4f0, 0x4f4, 0x4f8, 0x4fc, 0x010, 0x014, 0x018, 0x01c,
	0x1e0, 0x1e4, 0x1e8, 0x1ec,
};

struct test_perf {
	const uint32_t *slots;
	uint32_t num_slots;
	const uint32_t *wl;
	uint32_t num_wl;
} perf;

#define HAS_OA_MMIO_TRIGGER(__d) \
	(IS_DG2(__d) || IS_PONTEVECCHIO(__d) || IS_METEORLAKE(__d) || IS_LUNARLAKE(__d))

static void perf_init_whitelist(void)
{
	perf.slots = nonpriv_slot_offsets;
	perf.num_slots = 20;
	perf.wl = oa_wl;
	perf.num_wl = ARRAY_SIZE(oa_wl);
}

static void
emit_oa_reg_read(struct intel_bb *ibb, struct intel_buf *dst, uint32_t offset,
		 uint32_t reg)
{
	intel_bb_add_intel_buf(ibb, dst, true);

	intel_bb_out(ibb, MI_STORE_REGISTER_MEM | 2);
	intel_bb_out(ibb, reg);
	intel_bb_emit_reloc(ibb, dst->handle,
			    I915_GEM_DOMAIN_INSTRUCTION,
			    I915_GEM_DOMAIN_INSTRUCTION,
			    offset, dst->addr.offset);
	intel_bb_out(ibb, lower_32_bits(offset));
	intel_bb_out(ibb, upper_32_bits(offset));
}

static void
emit_mmio_triggered_report(struct intel_bb *ibb, uint32_t value)
{
	intel_bb_out(ibb, MI_LOAD_REGISTER_IMM(1));
	intel_bb_out(ibb, GEN12_OAG_MMIOTRIGGER);
	intel_bb_out(ibb, value);
}

static void dump_whitelist(uint32_t mmio_base, const char *msg)
{
	int i;

	igt_debug("%s\n", msg);

	for (i = 0; i < perf.num_slots; i++)
		igt_debug("FORCE_TO_NON_PRIV_%02d = %08x\n",
			  i, intel_register_read(&mmio_data,
						 mmio_base + perf.slots[i]));
}

static bool in_whitelist(uint32_t mmio_base, uint32_t reg)
{
	int i;

	if (reg & MMIO_BASE_OFFSET)
		reg = (reg & ~MMIO_BASE_OFFSET) + mmio_base;

	for (i = 0; i < perf.num_slots; i++) {
		uint32_t fpriv = intel_register_read(&mmio_data,
						     mmio_base + perf.slots[i]);

		if ((fpriv & RING_FORCE_TO_NONPRIV_ADDRESS_MASK) == reg)
			return true;
	}

	return false;
}

static void oa_regs_in_whitelist(uint32_t mmio_base, bool are_present)
{
	int i;

	if (are_present) {
		for (i = 0; i < perf.num_wl; i++)
			igt_assert(in_whitelist(mmio_base, perf.wl[i]));
	} else {
		for (i = 0; i < perf.num_wl; i++)
			igt_assert(!in_whitelist(mmio_base, perf.wl[i]));
	}
}

static u32 oa_get_mmio_base(struct drm_xe_engine_class_instance *hwe)
{
	u32 mmio_base = 0x2000;

	switch (hwe->engine_class) {
	case DRM_XE_ENGINE_CLASS_RENDER:
		mmio_base = 0x2000;
		break;
	case DRM_XE_ENGINE_CLASS_COMPUTE:
		switch (hwe->engine_instance) {
		case 0:
			mmio_base = 0x1a000;
			break;
		case 1:
			mmio_base = 0x1c000;
			break;
		case 2:
			mmio_base = 0x1e000;
			break;
		case 3:
			mmio_base = 0x26000;
			break;
		}
	}

	return mmio_base;
}

static void test_oa_regs_whitelist(const struct intel_execution_engine2 *e,
				   struct drm_xe_engine_class_instance *hwe)
{
	struct intel_perf_metric_set *test_set = metric_set(e);
	uint64_t properties[] = {
		DRM_XE_OA_PROPERTY_OA_UNIT_ID, 0,
		DRM_XE_OA_PROPERTY_SAMPLE_OA, true,
		DRM_XE_OA_PROPERTY_OA_METRIC_SET, test_set->perf_oa_metrics_set,
		DRM_XE_OA_PROPERTY_OA_FORMAT, __ff(test_set->perf_oa_format),
		DRM_XE_OA_PROPERTY_OA_EXPONENT, oa_exp_1_millisec,
	};
	struct drm_xe_oa_open_prop param = {
		.num_properties = sizeof(properties) / 16,
		.properties_ptr = to_user_pointer(properties),
	};
	// uint32_t mmio_base = gem_engine_mmio_base(drm_fd, e->name);
	u32 mmio_base;

	/* FIXME: Add support for OAM whitelist testing */
	if (hwe->engine_class != DRM_XE_ENGINE_CLASS_RENDER &&
	    hwe->engine_class != DRM_XE_ENGINE_CLASS_COMPUTE)
		return;

	mmio_base = oa_get_mmio_base(hwe);

	intel_register_access_init(&mmio_data,
				   igt_device_get_pci_device(drm_fd),
				   0, drm_fd);
	stream_fd = __perf_open(drm_fd, &param, false);

	dump_whitelist(mmio_base, "oa whitelisted");

	oa_regs_in_whitelist(mmio_base, true);

	__perf_close(stream_fd);

	dump_whitelist(mmio_base, "oa remove whitelist");

	/*
	 * after perf close, check that registers are removed from the nonpriv
	 * slots
	 * FIXME if needed: currently regs remain added forever
	 */
	// oa_regs_in_whitelist(mmio_base, false);

	intel_register_access_fini(&mmio_data);
}

static void
__test_mmio_triggered_reports(struct drm_xe_engine_class_instance *hwe)
{
	struct intel_perf_metric_set *test_set = default_test_set;
	int oa_exponent = max_oa_exponent_for_period_lte(2 * NSEC_PER_SEC);
	uint64_t properties[] = {
		DRM_XE_OA_PROPERTY_SAMPLE_OA, true,
		DRM_XE_OA_PROPERTY_OA_METRIC_SET, test_set->perf_oa_metrics_set,
		DRM_XE_OA_PROPERTY_OA_FORMAT, __ff(test_set->perf_oa_format),
		DRM_XE_OA_PROPERTY_OA_EXPONENT, oa_exponent,
		DRM_XE_OA_PROPERTY_OA_ENGINE_INSTANCE, hwe->engine_instance,
	};
	struct drm_xe_oa_open_prop param = {
		.num_properties = sizeof(properties) / 16,
		.properties_ptr = to_user_pointer(properties),
	};
	size_t format_size = get_oa_format(test_set->perf_oa_format).size;
	uint32_t oa_buffer, offset_tail1, offset_tail2;
	struct intel_buf src, dst, *dst_buf;
	uint32_t mmio_triggered_reports = 0;
	uint32_t *start, *end;
	struct buf_ops *bops;
	struct intel_bb *ibb;
	uint32_t context, vm;
	int height = 600;
	int width = 800;
	uint8_t *buf;

	bops = buf_ops_create(drm_fd);

	dst_buf = intel_buf_create(bops, 4096, 1, 8, 64,
				   I915_TILING_NONE,
				   I915_COMPRESSION_NONE);
	buf_map(drm_fd, dst_buf, true);
	memset(dst_buf->ptr, 0, 4096);
	intel_buf_unmap(dst_buf);

	scratch_buf_init(bops, &src, width, height, 0xff0000ff);
	scratch_buf_init(bops, &dst, width, height, 0x00ff00ff);

	if (is_xe_device(drm_fd)) {
		vm = xe_vm_create(drm_fd, 0, 0);
		context = xe_exec_queue_create(drm_fd, vm, hwe, 0);
	} else {
		context = gem_context_create(drm_fd);
	}
	igt_assert(context);
	ibb = intel_bb_create_with_context(drm_fd, context, vm, NULL, BATCH_SZ);

	stream_fd = __perf_open(drm_fd, &param, false);
        set_fd_flags(stream_fd, O_CLOEXEC);

	buf = mmap(0, OA_BUFFER_SIZE, PROT_READ, MAP_PRIVATE, stream_fd, 0);
	igt_assert(buf != NULL);

	emit_oa_reg_read(ibb, dst_buf, 0, GEN12_OAG_OABUFFER);
	emit_oa_reg_read(ibb, dst_buf, 4, GEN12_OAG_OATAILPTR);
	emit_mmio_triggered_report(ibb, 0xc0ffee11);

	if (render_copy)
		render_copy(ibb,
			    &src, 0, 0, width, height,
			    &dst, 0, 0);

	emit_mmio_triggered_report(ibb, 0xc0ffee22);

	emit_oa_reg_read(ibb, dst_buf, 8, GEN12_OAG_OATAILPTR);

	intel_bb_flush_render(ibb);
	intel_bb_sync(ibb);

	buf_map(drm_fd, dst_buf, false);

	oa_buffer = dst_buf->ptr[0] & GEN12_OAG_OATAILPTR_MASK;
	offset_tail1 = (dst_buf->ptr[1] & GEN12_OAG_OATAILPTR_MASK) - oa_buffer;
	offset_tail2 = (dst_buf->ptr[2] & GEN12_OAG_OATAILPTR_MASK) - oa_buffer;

	igt_debug("oa_buffer = %08x, tail1 = %08x, tail2 = %08x\n",
		  oa_buffer, offset_tail1, offset_tail2);

	start = (uint32_t *)(buf + offset_tail1);
	end = (uint32_t *)(buf + offset_tail2);
	while (start < end) {
		if (!gen8_report_reason(start))
			mmio_triggered_reports++;

		if (get_oa_format(test_set->perf_oa_format).report_hdr_64bit) {
			u64 *start64 = (u64 *)start;

			igt_debug("hdr: %016lx %016lx %016lx %016lx\n",
				  start64[0], start64[1], start64[2], start64[3]);
		} else {
			igt_debug("hdr: %08x %08x %08x %08x\n",
				  start[0], start[1], start[2], start[3]);
		}

		start += format_size / 4;
	}

	igt_assert_eq(mmio_triggered_reports, 2);

	munmap(buf, OA_BUFFER_SIZE);
	intel_buf_close(bops, &src);
	intel_buf_close(bops, &dst);
	intel_buf_unmap(dst_buf);
	intel_buf_destroy(dst_buf);
	intel_bb_destroy(ibb);
	if (is_xe_device(drm_fd)) {
		xe_exec_queue_destroy(drm_fd, context);
		xe_vm_destroy(drm_fd, vm);
	} else {
		gem_context_destroy(drm_fd, context);
	}
	buf_ops_destroy(bops);
	__perf_close(stream_fd);
}

static void
test_mmio_triggered_reports(struct drm_xe_engine_class_instance *hwe)
{
	struct igt_helper_process child = {};
	int ret;

	write_u64_file("/proc/sys/dev/xe/perf_stream_paranoid", 0);
	igt_fork_helper(&child) {
		igt_drop_root();

		__test_mmio_triggered_reports(hwe);
	}
	ret = igt_wait_helper(&child);
	write_u64_file("/proc/sys/dev/xe/perf_stream_paranoid", 1);

	igt_assert(WEXITSTATUS(ret) == EAGAIN ||
		   WEXITSTATUS(ret) == 0);
}

static unsigned
read_i915_module_ref(bool is_xe)
{
	FILE *fp = fopen("/proc/modules", "r");
	char *line = NULL;
	size_t line_buf_size = 0;
	int len = 0;
	unsigned ref_count;
	char mod[8];
	int modn = is_xe ? 3 : 5;

	igt_assert(fp);

	strcpy(mod, is_xe ? "xe " : "i915 ");
	while ((len = getline(&line, &line_buf_size, fp)) > 0) {
		if (strncmp(line, mod, modn) == 0) {
			unsigned long mem;
			int ret = sscanf(line + 5, "%lu %u", &mem, &ref_count);
			igt_assert(ret == 2);
			goto done;
		}
	}

	igt_assert(!"reached");

done:
	free(line);
	fclose(fp);
	return ref_count;
}

static int perf_sysfs_open(int i915)
{
	int dirfd, gt;

	if (is_xe_device(drm_fd))
		return igt_sysfs_open(drm_fd);

	/* use the first available sysfs interface */
	for_each_sysfs_gt_dirfd(i915, dirfd, gt)
		break;

	igt_assert(dirfd != -1);

	return dirfd;
}

/* check that an open i915 perf stream holds a reference on the drm i915 module
 * including in the corner case where the original drm fd has been closed.
 */
static void
test_xe_ref_count(void)
{
	uint64_t properties[] = {
		DRM_XE_OA_PROPERTY_OA_UNIT_ID, 0,

		/* Include OA reports in samples */
		DRM_XE_OA_PROPERTY_SAMPLE_OA, true,

		/* OA unit configuration */
		DRM_XE_OA_PROPERTY_OA_METRIC_SET, 0 /* updated below */,
		DRM_XE_OA_PROPERTY_OA_FORMAT, __ff(0), /* update below */
		DRM_XE_OA_PROPERTY_OA_EXPONENT, 0, /* update below */
	};
	struct drm_xe_oa_open_prop param = {
		.num_properties = ARRAY_SIZE(properties) / 2,
		.properties_ptr = to_user_pointer(properties),
	};
	unsigned baseline, ref_count0, ref_count1;
	uint32_t oa_report0[64];
	uint32_t oa_report1[64];
	bool is_xe;

	/* This should be the first test before the first fixture so no drm_fd
	 * should have been opened so far...
	 */
	igt_assert_eq(drm_fd, -1);

	/* Tell read_i915_module_ref if we are on xe or i915 (because drm_fd is -1) */
	drm_fd = __drm_open_driver(DRIVER_INTEL | DRIVER_XE);
	is_xe = is_xe_device(drm_fd);
	drm_close_driver(drm_fd);
	close(sysfs);
	drm_fd = -1;

	baseline = read_i915_module_ref(is_xe);
	igt_debug("baseline ref count (drm fd closed) = %u\n", baseline);

	drm_fd = __drm_open_driver(DRIVER_INTEL | DRIVER_XE);
	if (is_xe_device(drm_fd))
		xe_device_get(drm_fd);
	devid = intel_get_drm_devid(drm_fd);
	sysfs = perf_sysfs_open(drm_fd);

	/* Note: these global variables are only initialized after calling
	 * init_sys_info()...
	 */
	igt_require(init_sys_info());
	properties[5] = default_test_set->perf_oa_metrics_set;
	properties[7] = __ff(default_test_set->perf_oa_format);
	properties[9] = oa_exp_1_millisec;

	ref_count0 = read_i915_module_ref(is_xe);
	igt_debug("initial ref count with drm_fd open = %u\n", ref_count0);
	igt_assert(ref_count0 > baseline);

	stream_fd = __perf_open(drm_fd, &param, false);
        set_fd_flags(stream_fd, O_CLOEXEC);
	ref_count1 = read_i915_module_ref(is_xe);
	igt_debug("ref count after opening i915 perf stream = %u\n", ref_count1);
	igt_assert(ref_count1 > ref_count0);

	drm_close_driver(drm_fd);
	close(sysfs);
	drm_fd = -1;
	sysfs = -1;
	ref_count0 = read_i915_module_ref(is_xe);
	igt_debug("ref count after closing drm fd = %u\n", ref_count0);

	igt_assert(ref_count0 > baseline);

	read_2_oa_reports(default_test_set->perf_oa_format,
			  oa_exp_1_millisec,
			  oa_report0,
			  oa_report1,
			  false); /* not just timer reports */

	__perf_close(stream_fd);
	ref_count0 = read_i915_module_ref(is_xe);
	igt_debug("ref count after closing i915 perf stream fd = %u\n", ref_count0);
	igt_assert_eq(ref_count0, baseline);
}

static void
test_sysctl_defaults(void)
{
	int paranoid = read_u64_file("/proc/sys/dev/xe/perf_stream_paranoid");

	igt_assert_eq(paranoid, 1);
}

static struct intel_execution_engine2 *
__ci_to_e2(const intel_ctx_t *ctx, struct i915_engine_class_instance *ci)
{
	static struct intel_execution_engine2 e2;
	struct intel_execution_engine2 *e;

	if (is_xe_device(drm_fd)) {
		strncpy(e2.name, "rcs", sizeof(e2.name));
		goto exit;
	}

	for_each_ctx_engine(drm_fd, ctx, e) {
		if (e->class == ci->engine_class && e->instance == ci->engine_instance) {
			e2 = *e;
			break;
		}
	}
exit:
	return &e2;
}

#define __for_random_engine_in_each_group(groups_, ctx_, e_) \
	for (int i_ = 0; \
	     i_ < num_perf_oa_groups && !!(e_ = __ci_to_e2(ctx_, random_engine(&groups_[i_]))); \
	     i_++) \
		igt_dynamic_f("%d-%s", i_, e_->name)

#define __for_each_render_engine(fd__, e__) \
	for_each_physical_engine(fd__, e__) \
		if (e__->class == DRM_XE_ENGINE_CLASS_RENDER) \
			igt_dynamic_f("%s", e__->name)

struct perf_engine_group {
	/* exclusive perf fd per engine group */
	int perf_fd;

	/* gem context id passed to perf */
	uint32_t ctx_id;
	uint32_t oa_unit_id;

	/* perf engines in a group */
	int num_engines;
	struct i915_engine_class_instance *ci;

	int gt;
	struct intel_perf_metric_set *test_set;
};

static struct drm_i915_query_engine_info *query_engine_info(int i915)
{
	struct drm_i915_query_engine_info *qinfo;

#define QUERY_SIZE (0x4000)
	qinfo = malloc(QUERY_SIZE);
	igt_assert(qinfo);
	memset(qinfo, 0, QUERY_SIZE);
	igt_assert(!__gem_query_engines(i915, qinfo, QUERY_SIZE));
#undef QUERY_SIZE

	return qinfo;
}

static struct intel_perf_metric_set *metric_set(const struct intel_execution_engine2 *e2)
{
	const char *test_set_name = NULL;
	struct intel_perf_metric_set *metric_set_iter;
	struct intel_perf_metric_set *test_set = NULL;

	if (IS_HASWELL(devid))
		test_set_name = "RenderBasic";
	else if (e2->class == DRM_XE_ENGINE_CLASS_RENDER)
		test_set_name = "TestOa";
	else if ((e2->class == DRM_XE_ENGINE_CLASS_VIDEO_DECODE ||
		  e2->class == DRM_XE_ENGINE_CLASS_VIDEO_ENHANCE) &&
		 HAS_OAM(devid))
		test_set_name = "MediaSet1";
	else
		igt_assert(!"reached");

	igt_list_for_each_entry(metric_set_iter, &intel_perf->metric_sets, link) {
		if (strcmp(metric_set_iter->symbol_name, test_set_name) == 0) {
			test_set = metric_set_iter;
			break;
		}
	}

	igt_assert(test_set);

	/*
	 * configuration was loaded in init_sys_info() ->
	 * intel_perf_load_perf_configs(), and test_set->perf_oa_metrics_set
	 * should point to metric id returned by the config add ioctl. 0 is
	 * invalid.
	 */
	igt_assert_neq_u64(test_set->perf_oa_metrics_set, 0);

	igt_debug("engine %d:%d - %s metric set UUID = %s\n",
		  e2->class,
		  e2->instance,
		  test_set->symbol_name,
		  test_set->hw_config_guid);

	return test_set;
}

static int compare_engine_oa_unit_id(const void *e1, const void *e2)
{
	const struct drm_i915_engine_info *_e1 = e1;
	const struct drm_i915_engine_info *_e2 = e2;

	return (int)_e1->rsvd0 - (int)_e2->rsvd0;
}

static struct perf_engine_group *default_engine_group(uint32_t *num_groups)
{
	struct perf_engine_group *groups = malloc(sizeof(*groups));

	igt_debug("using default engine group\n");

	groups->perf_fd = -1,
	groups->ctx_id = 0xffffffff,
	groups->oa_unit_id = 0,
	groups->num_engines = 1,

	groups->ci = malloc(sizeof(*groups->ci));
	groups->ci->engine_class = default_e2.class;
	groups->ci->engine_instance = default_e2.instance;

	*num_groups = 1;

	return groups;
}

/* Until oa_unit_id is exposed from uapi, work around it */
static void populate_mtl_oa_unit_ids(struct drm_i915_query_engine_info *qinfo)
{
	struct i915_engine_class_instance ci;
	int i;

	for (i = 0; i < qinfo->num_engines; i++) {
		ci = qinfo->engines[i].engine;

		switch (ci.engine_class) {
		case DRM_XE_ENGINE_CLASS_RENDER:
			qinfo->engines[i].rsvd0 = 0;
			break;

		case DRM_XE_ENGINE_CLASS_VIDEO_DECODE:
		case DRM_XE_ENGINE_CLASS_VIDEO_ENHANCE:
			qinfo->engines[i].rsvd0 = 1;
			break;

		default:
			qinfo->engines[i].rsvd0 = UINT32_MAX;
			break;
		}

		igt_debug("class:instance = %d:%d, id = %d\n",
			  ci.engine_class, ci.engine_instance,
			  qinfo->engines[i].rsvd0);
	}
}

static struct perf_engine_group *get_engine_groups(int i915, uint32_t *num_groups)
{
	struct drm_i915_query_engine_info *qinfo;
	struct perf_engine_group *groups = NULL;
	uint32_t id = UINT32_MAX, num_grps = 0, i = 0, j;

	if (is_xe_device(drm_fd))
		return default_engine_group(num_groups);

	qinfo = query_engine_info(i915);
	if (!qinfo)
		return default_engine_group(num_groups);
	igt_assert(qinfo->num_engines);

	/* Currently only meteorlake is supported with engine groups */
	if (IS_METEORLAKE(devid)) {
		populate_mtl_oa_unit_ids(qinfo);
	} else {
		free(qinfo);
		return default_engine_group(num_groups);
	}

	/* sort so that engines with same oa id are together */
	qsort(qinfo->engines, qinfo->num_engines, sizeof(qinfo->engines[0]),
	      compare_engine_oa_unit_id);

	/* create groups */
	for (i = 0; i < qinfo->num_engines; i++) {
		struct i915_engine_class_instance ci = qinfo->engines[i].engine;

		igt_debug("class:instance = %d:%d, id = %d\n",
			  ci.engine_class, ci.engine_instance,
			  qinfo->engines[i].rsvd0);

		if (qinfo->engines[i].rsvd0 == UINT32_MAX)
			continue;

		if (qinfo->engines[i].rsvd0 != id) {
			id = qinfo->engines[i].rsvd0;
			groups = realloc(groups, ++num_grps * sizeof(*groups));
			j = num_grps - 1;
			groups[j].perf_fd = -1;
			groups[j].ctx_id = 0xffffffff;
			groups[j].oa_unit_id = id;
			groups[j].num_engines = 0;
			/* alloc max engines, trim later */
			groups[j].ci = malloc(qinfo->num_engines * sizeof(ci));
		}
		groups[j].ci[groups[j].num_engines++] = ci;
	}

	igt_assert(num_grps);

	/* trim engines */
	for (i = 0; i < num_grps; i++) {
		struct i915_engine_class_instance *ci = groups[i].ci;

		ci = realloc(ci, groups[i].num_engines * sizeof(*ci));
		groups[i].ci = ci;
	}

	*num_groups = num_grps;

	free(qinfo);

	return groups;
}

static void put_engine_groups(struct perf_engine_group *groups,
			      unsigned int num_groups)
{
	int i;

	for (i = 0; i < num_groups; i++)
		free(groups[i].ci);

	free(groups);
}

static struct i915_engine_class_instance *
random_engine(struct perf_engine_group *group)
{
	srandom(time(NULL));

	return &group->ci[random() % group->num_engines];
}

static bool has_class_instance(int i915, uint16_t class, uint16_t instance)
{
	if (class == DRM_XE_ENGINE_CLASS_RENDER && !IS_PONTEVECCHIO(devid))
		return true;
	return false;
}

static void set_default_engine(const intel_ctx_t *ctx)
{
	const struct intel_execution_engine2 *e;

	for_each_ctx_engine(drm_fd, ctx, e)
		if (e->class == DRM_XE_ENGINE_CLASS_RENDER && e->instance == 0)
			default_e2 = *e;
}

/*
 * Test if OA buffer streams can be independently opened on each group. Once a user
 * opens a stream, that group is exclusive to the user, other users get -EBUSY on
 * trying to open a stream.
 */
static void
test_group_exclusive_stream(const intel_ctx_t *ctx, bool exponent)
{
	uint64_t properties[] = {
		DRM_XE_OA_PROPERTY_OA_UNIT_ID, 0,
		DRM_XE_OA_PROPERTY_SAMPLE_OA, true,
		DRM_XE_OA_PROPERTY_OA_METRIC_SET, 0,
		DRM_XE_OA_PROPERTY_OA_FORMAT, __ff(0),
		DRM_XE_OA_PROPERTY_OA_ENGINE_INSTANCE, 0,
		DRM_XE_OA_PROPERTY_OA_EXPONENT, oa_exp_1_millisec,
	};
	struct drm_xe_oa_open_prop param = {
		/* for gem_context use case, we do no pass exponent */
		.num_properties = exponent ?
				  ARRAY_SIZE(properties) / 2 :
				  ARRAY_SIZE(properties) / 2 - 1,
		.properties_ptr = to_user_pointer(properties),
	};
	uint32_t i, j;

	/* for each group, open one random perf stream with sample OA */
	for (i = 0; i < num_perf_oa_groups; i++) {
		struct perf_engine_group *grp = &perf_oa_groups[i];
		struct i915_engine_class_instance *ci = random_engine(grp);
		struct intel_execution_engine2 *e2 = __ci_to_e2(ctx, ci);
		struct intel_perf_metric_set *test_set = metric_set(e2);

		if (!exponent) {
			properties[0] = DRM_XE_OA_PROPERTY_EXEC_QUEUE_ID;
			properties[1] = ctx->id;
		}

		properties[5] = test_set->perf_oa_metrics_set;
		properties[7] = __ff(test_set->perf_oa_format);
		properties[9] = ci->engine_instance;
		grp->perf_fd = xe_perf_ioctl(drm_fd, DRM_IOCTL_XE_PERF,
					     DRM_XE_PERF_OP_STREAM_OPEN, &param);
		igt_assert(grp->perf_fd >= 0);
		igt_debug("opened OA buffer with c:i %d:%d\n",
			  ci->engine_class, ci->engine_instance);
	}

	/* for each group make sure no other streams can be opened */
	for (i = 0; i < num_perf_oa_groups; i++) {
		struct perf_engine_group *grp = &perf_oa_groups[i];
		int err;

		for (j = 0; j < grp->num_engines; j++) {
			struct i915_engine_class_instance *ci = grp->ci + j;
			struct intel_execution_engine2 *e2 = __ci_to_e2(ctx, ci);
			struct intel_perf_metric_set *test_set = metric_set(e2);

			/*
			 * case 1:
			 * concurrent access to OAG should fail
			 */
			properties[0] = DRM_XE_OA_PROPERTY_SAMPLE_OA;
			properties[1] = true;
			properties[3] = test_set->perf_oa_metrics_set;
			properties[5] = __ff(test_set->perf_oa_format);
			properties[7] = ci->engine_class;
			properties[9] = ci->engine_instance;
			/* for SAMPLE OA use case, we must pass exponent */
			param.num_properties = ARRAY_SIZE(properties) / 2;
			xe_perf_ioctl_err(drm_fd, DRM_IOCTL_XE_PERF, DRM_XE_PERF_OP_STREAM_OPEN,
					  &param, EBUSY);
			igt_debug("try OA buffer with c:i %d:%d\n",
				  ci->engine_class, ci->engine_instance);

			/*
			 * case 2:
			 * concurrent access to non-OAG unit should fail
			 */
			properties[0] = DRM_XE_OA_PROPERTY_EXEC_QUEUE_ID;
			properties[1] = gem_context_create(drm_fd);
			/* for gem_context use case, we do no pass exponent */
			param.num_properties = ARRAY_SIZE(properties) / 2 - 1;
			errno = 0;
			err = xe_perf_ioctl(drm_fd, DRM_IOCTL_XE_PERF,
					    DRM_XE_PERF_OP_STREAM_OPEN, &param);
			igt_assert(err < 0);
			igt_assert(errno == EBUSY || errno == ENODEV);
			igt_debug("try OA ci unit with c:i %d:%d\n",
				  ci->engine_class, ci->engine_instance);
			gem_context_destroy(drm_fd, properties[1]);
		}

		if (grp->perf_fd >= 0)
			close(grp->perf_fd);
	}
}

static void
test_group_concurrent_oa_buffer_read(void)
{
	igt_fork(child, num_perf_oa_groups) {
		struct intel_execution_engine2 e;

		e.class = perf_oa_groups[child].ci->engine_class;
		e.instance = perf_oa_groups[child].ci->engine_instance;

		test_blocking(40 * 1000 * 1000, false, 5 * 1000 * 1000, &e);
	}
	igt_waitchildren();
}

static void *map_oa_buffer(u32 *size)
{
	void *vaddr = mmap(0, OA_BUFFER_SIZE, PROT_READ, MAP_PRIVATE, stream_fd, 0);

	igt_assert(vaddr != NULL);
	*size = OA_BUFFER_SIZE;
	return vaddr;
}

static void invalid_param_map_oa_buffer(const struct intel_execution_engine2 *e)
{
	void *oa_vaddr = NULL;

	/* try a couple invalid mmaps */
	/* bad prots */
	oa_vaddr = mmap(0, OA_BUFFER_SIZE, PROT_WRITE, MAP_PRIVATE, stream_fd, 0);
	igt_assert(oa_vaddr == MAP_FAILED);

	oa_vaddr = mmap(0, OA_BUFFER_SIZE, PROT_EXEC, MAP_PRIVATE, stream_fd, 0);
	igt_assert(oa_vaddr == MAP_FAILED);

	/* bad MAPs */
	oa_vaddr = mmap(0, OA_BUFFER_SIZE, PROT_READ, MAP_SHARED, stream_fd, 0);
	igt_assert(oa_vaddr == MAP_FAILED);

	/* bad size */
	oa_vaddr = mmap(0, OA_BUFFER_SIZE + 1, PROT_READ, MAP_PRIVATE, stream_fd, 0);
	igt_assert(oa_vaddr == MAP_FAILED);

	/* do the right thing */
	oa_vaddr = mmap(0, OA_BUFFER_SIZE, PROT_READ, MAP_PRIVATE, stream_fd, 0);
	igt_assert(oa_vaddr != MAP_FAILED && oa_vaddr != NULL);

	munmap(oa_vaddr, OA_BUFFER_SIZE);
}

static void unprivileged_try_to_map_oa_buffer(void)
{
	void *oa_vaddr;

	oa_vaddr = mmap(0, OA_BUFFER_SIZE, PROT_READ, MAP_PRIVATE, stream_fd, 0);
	igt_assert(oa_vaddr == MAP_FAILED);
	igt_assert_eq(errno, EACCES);
}

static void unprivileged_map_oa_buffer(const struct intel_execution_engine2 *e)
{
	igt_fork(child, 1) {
		igt_drop_root();
		unprivileged_try_to_map_oa_buffer();
	}
	igt_waitchildren();
}

static jmp_buf jmp;
static void __attribute__((noreturn)) sigtrap(int sig)
{
	siglongjmp(jmp, sig);
}

static void try_invalid_access(void *vaddr)
{
	sighandler_t old_sigsegv;
	uint32_t dummy;

	old_sigsegv = signal(SIGSEGV, sigtrap);
	switch (sigsetjmp(jmp, SIGSEGV)) {
	case SIGSEGV:
		break;
	case 0:
		dummy = READ_ONCE(*((uint32_t *)vaddr + 1));
		(void) dummy;
	default:
		igt_assert(!"reached");
		break;
	}
	signal(SIGSEGV, old_sigsegv);
}

static void map_oa_buffer_unprivilege_access(const struct intel_execution_engine2 *e)
{
	void *vaddr;
	uint32_t size;

	vaddr = map_oa_buffer(&size);

	igt_fork(child, 1) {
		igt_drop_root();
		try_invalid_access(vaddr);
	}
	igt_waitchildren();

	munmap(vaddr, size);
}

static void map_oa_buffer_forked_access(const struct intel_execution_engine2 *e)
{
	void *vaddr;
	uint32_t size;

	vaddr = map_oa_buffer(&size);

	igt_fork(child, 1) {
		try_invalid_access(vaddr);
	}
	igt_waitchildren();

	munmap(vaddr, size);
}

static void check_reports(void *oa_vaddr, uint32_t oa_size,
			  const struct intel_execution_engine2 *e)
{
	struct intel_perf_metric_set *test_set = metric_set(e);
	uint64_t fmt = test_set->perf_oa_format;
	struct oa_format format = get_oa_format(fmt);
	size_t report_words = format.size >> 2;
	uint32_t *reports;
	uint32_t timer_reports = 0;

	for (reports = (uint32_t *)oa_vaddr;
	     timer_reports < 20 && reports[0] && oa_timestamp(reports, fmt);
	     reports += report_words) {
		if (!oa_report_is_periodic(oa_exp_1_millisec, reports))
			continue;

		timer_reports++;
		if (timer_reports >= 3)
			sanity_check_reports(reports - 2 * report_words,
					     reports - report_words, fmt);
	}

	igt_assert(timer_reports >= 3);
}

static void check_reports_from_mapped_buffer(const struct intel_execution_engine2 *e)
{
	void *vaddr;
	uint32_t size;
	uint32_t period_us = oa_exponent_to_ns(oa_exp_1_millisec) / 1000;

	vaddr = map_oa_buffer(&size);

	/* wait for approx 100 reports */
	usleep(100 * period_us);
	check_reports(vaddr, size, e);

	munmap(vaddr, size);
}

static void closed_fd_and_unmapped_access(const struct intel_execution_engine2 *e)
{
	uint64_t properties[] = {
		DRM_XE_OA_PROPERTY_OA_UNIT_ID, 0,
		DRM_XE_OA_PROPERTY_SAMPLE_OA, true,
		DRM_XE_OA_PROPERTY_OA_METRIC_SET, default_test_set->perf_oa_metrics_set,
		DRM_XE_OA_PROPERTY_OA_FORMAT, __ff(default_test_set->perf_oa_format),
		DRM_XE_OA_PROPERTY_OA_EXPONENT, oa_exp_1_millisec,
	};
	struct drm_xe_oa_open_prop param = {
		.num_properties = has_param_class_instance() ?
				  ARRAY_SIZE(properties) / 2 :
				  (ARRAY_SIZE(properties) / 2) - 2,
		.properties_ptr = to_user_pointer(properties),
	};
	void *vaddr;
	uint32_t size;
	uint32_t period_us = oa_exponent_to_ns(oa_exp_1_millisec) / 1000;

	stream_fd = __perf_open(drm_fd, &param, false);
	vaddr = map_oa_buffer(&size);

	usleep(100 * period_us);
	check_reports(vaddr, size, e);

	munmap(vaddr, size);
	__perf_close(stream_fd);

	try_invalid_access(vaddr);
}

typedef void (*map_oa_buffer_test_t)(const struct intel_execution_engine2 *e);
static void test_mapped_oa_buffer(map_oa_buffer_test_t test_with_fd_open,
				  const struct intel_execution_engine2 *e)
{
	struct intel_perf_metric_set *test_set = metric_set(e);
	uint64_t properties[] = {
		DRM_XE_OA_PROPERTY_OA_UNIT_ID, 0,
		DRM_XE_OA_PROPERTY_SAMPLE_OA, true,
		DRM_XE_OA_PROPERTY_OA_METRIC_SET, test_set->perf_oa_metrics_set,
		DRM_XE_OA_PROPERTY_OA_FORMAT, __ff(test_set->perf_oa_format),
		DRM_XE_OA_PROPERTY_OA_EXPONENT, oa_exp_1_millisec,
		DRM_XE_OA_PROPERTY_OA_ENGINE_INSTANCE, e->instance,
	};
	struct drm_xe_oa_open_prop param = {
		.num_properties = ARRAY_SIZE(properties) / 2,
		.properties_ptr = to_user_pointer(properties),
	};

	stream_fd = __perf_open(drm_fd, &param, false);

	igt_assert(test_with_fd_open);
	test_with_fd_open(e);

	__perf_close(stream_fd);
}

igt_main
{
	const intel_ctx_t *ctx = NULL;
	const struct intel_execution_engine2 *e;
	struct drm_xe_engine_class_instance *hwe;

	igt_fixture {
		struct stat sb;

		/*
		 * Prior tests may have unloaded the module or failed while
		 * loading/unloading the module. Load xe here before we
		 * stat the files.
		 */
		drm_load_module(DRIVER_XE);

		igt_require(stat("/proc/sys/dev/xe/perf_stream_paranoid", &sb)
			    == 0);
	}

	igt_subtest("xe-ref-count")
		test_xe_ref_count();

	igt_subtest("sysctl-defaults")
		test_sysctl_defaults();

	igt_fixture {
		/* We expect that the ref count test before these fixtures
		 * should have closed drm_fd...
		 */
		igt_assert_eq(drm_fd, -1);

		/* Avoid the normal exithandler, our perf-fd interferes */
		drm_fd = __drm_open_driver(DRIVER_XE);
		xe_device_get(drm_fd);

		devid = intel_get_drm_devid(drm_fd);
		sysfs = perf_sysfs_open(drm_fd);

		igt_require(init_sys_info());

		if (!is_xe_device(drm_fd)) {
			ctx = intel_ctx_create_all_physical(drm_fd);
			set_default_engine(ctx);
		}

		write_u64_file("/proc/sys/dev/xe/perf_stream_paranoid", 1);

		if (!xe_relax_checks(drm_fd))
			gt_max_freq_mhz = sysfs_read(RPS_RP0_FREQ_MHZ);
		perf_oa_groups = get_engine_groups(drm_fd, &num_perf_oa_groups);
		igt_assert(perf_oa_groups && num_perf_oa_groups);

		if (has_class_instance(drm_fd, DRM_XE_ENGINE_CLASS_RENDER, 0))
			render_copy = igt_get_render_copyfunc(devid);
	}

	igt_subtest("non-system-wide-paranoid")
		test_system_wide_paranoid();

	igt_subtest("invalid-oa-metric-set-id")
		test_invalid_oa_metric_set_id();

	igt_subtest("invalid-oa-format-id")
		test_invalid_oa_format_id();

	igt_subtest("missing-sample-flags")
		test_missing_sample_flags();

	igt_subtest_with_dynamic("oa-formats")
		__for_random_engine_in_each_group(perf_oa_groups, ctx, e)
			test_oa_formats(e);

	igt_subtest("invalid-oa-exponent")
		test_invalid_oa_exponent();
	igt_subtest("low-oa-exponent-permissions") {
		igt_require(!is_xe_device(drm_fd));
		test_low_oa_exponent_permissions();
	}
	igt_subtest_with_dynamic("oa-exponents")
		__for_random_engine_in_each_group(perf_oa_groups, ctx, e)
			test_oa_exponents(e);

	igt_subtest_with_dynamic("buffer-fill")
		__for_random_engine_in_each_group(perf_oa_groups, ctx, e)
			test_buffer_fill(e);

	igt_describe("Test that reason field in OA reports is never 0 on Gen8+");
	igt_subtest_with_dynamic("non-zero-reason") {
		/* Reason field is only available on Gen8+ */
		igt_require(intel_gen(devid) >= 8);
		__for_random_engine_in_each_group(perf_oa_groups, ctx, e)
			test_non_zero_reason(e);
	}

	igt_subtest("disabled-read-error")
		test_disabled_read_error();
	igt_subtest("non-sampling-read-error")
		test_non_sampling_read_error();

	igt_subtest_with_dynamic("enable-disable")
		__for_random_engine_in_each_group(perf_oa_groups, ctx, e)
			test_enable_disable(e);

	igt_describe("Test blocking read with default hrtimer frequency");
	igt_subtest_with_dynamic("blocking") {
		__for_random_engine_in_each_group(perf_oa_groups, ctx, e)
			test_blocking(40 * 1000 * 1000 /* 40ms oa period */,
				      false /* set_kernel_hrtimer */,
				      5 * 1000 * 1000 /* default 5ms/200Hz hrtimer */,
				      e);
	}

	igt_describe("Test polled read with default hrtimer frequency");
	igt_subtest_with_dynamic("polling") {
		__for_random_engine_in_each_group(perf_oa_groups, ctx, e)
			test_polling(40 * 1000 * 1000 /* 40ms oa period */,
				     false /* set_kernel_hrtimer */,
				     5 * 1000 * 1000 /* default 5ms/200Hz hrtimer */,
				     e);
	}

	igt_describe("Test polled read with buffer size smaller than available data");
	igt_subtest("polling-small-buf")
		test_polling_small_buf();

	igt_subtest("short-reads") {
		igt_require(!xe_relax_checks(drm_fd));
		test_short_reads();
	}

	igt_subtest_group {
		igt_fixture igt_require(intel_gen(devid) >= 12);

		igt_describe("Test MI REPORT PERF COUNT for Gen 12");
		igt_subtest_with_dynamic("gen12-mi-rpc") {
			const struct intel_execution_engine2 e2 = {};

			xe_for_each_engine(drm_fd, hwe) {
				if (hwe->engine_class == DRM_XE_ENGINE_CLASS_RENDER)
					break;
			}
			igt_dynamic_f("%s", "rcs") {
				igt_require(hwe->engine_class == DRM_XE_ENGINE_CLASS_RENDER);
				gen12_test_mi_rpc(&e2, hwe);
			}

			xe_for_each_engine(drm_fd, hwe) {
				if (hwe->engine_class == DRM_XE_ENGINE_CLASS_COMPUTE)
					break;
			}
			igt_dynamic_f("%s", "ccs") {
				igt_require(hwe->engine_class == DRM_XE_ENGINE_CLASS_COMPUTE);
				gen12_test_mi_rpc(&e2, hwe);
			}
		}

		igt_describe("Test OA TLB invalidate");
		igt_subtest_with_dynamic("gen12-oa-tlb-invalidate")
			__for_random_engine_in_each_group(perf_oa_groups, ctx, e)
				gen12_test_oa_tlb_invalidate(e);

		igt_describe("Measure performance for a specific context using OAR in Gen 12");
		igt_subtest_with_dynamic("gen12-unprivileged-single-ctx-counters") {
			igt_require(has_class_instance(drm_fd, DRM_XE_ENGINE_CLASS_RENDER, 0));
			igt_require_f(render_copy, "no render-copy function\n");
			igt_require(!IS_LUNARLAKE(devid));
			if (is_xe_device(drm_fd)) {
				const struct intel_execution_engine2 e2 = {};
				igt_dynamic_f("%s", "rcs")
					gen12_test_single_ctx_render_target_writes_a_counter(&e2, false);
			} else {
				__for_each_render_engine(drm_fd, e)
					gen12_test_single_ctx_render_target_writes_a_counter(e, false);
			}
		}
		igt_subtest_with_dynamic("gen12-unprivileged-one-ctx") {
			igt_require(has_class_instance(drm_fd, DRM_XE_ENGINE_CLASS_RENDER, 0));
			igt_require_f(render_copy, "no render-copy function\n");
			igt_require(!IS_LUNARLAKE(devid));
			if (is_xe_device(drm_fd)) {
				const struct intel_execution_engine2 e2 = {};
				igt_dynamic_f("%s", "rcs")
					gen12_test_single_ctx_render_target_writes_a_counter(&e2, true);
			}
		}
	}

	igt_subtest_group {
		/*
		 * OAR and OAG use cases can be separately opened only on gen12
		 * and later, so group-exclusive-stream tests require >= 12.
		 */
		igt_describe("Verify exclusivity of perf streams with sample oa option");
		igt_subtest("gen12-group-exclusive-stream-sample-oa") {
			igt_require(!is_xe_device(drm_fd));
			igt_require(intel_gen(devid) >= 12);
			test_group_exclusive_stream(ctx, true);
		}

		igt_describe("Verify exclusivity of perf streams with ctx handle");
		igt_subtest("gen12-group-exclusive-stream-ctx-handle") {
			igt_require(!is_xe_device(drm_fd));
			igt_require(intel_gen(devid) >= 12);
			test_group_exclusive_stream(ctx, false);
		}

		igt_describe("Verify concurrent reads from OA buffers in different groups");
		igt_subtest("gen12-group-concurrent-oa-buffer-read")
			test_group_concurrent_oa_buffer_read();
	}

	igt_subtest("rc6-disable")
		test_rc6_disable();

	igt_describe("Stress tests opening & closing the xe-oa stream in a busy loop");
	igt_subtest_with_dynamic("stress-open-close") {
		igt_require(!IS_LUNARLAKE(devid));
		__for_random_engine_in_each_group(perf_oa_groups, ctx, e)
			test_stress_open_close(e);
	}

	igt_subtest("invalid-create-userspace-config")
		test_invalid_create_userspace_config();

	igt_subtest("invalid-remove-userspace-config")
		test_invalid_remove_userspace_config();

	igt_subtest("create-destroy-userspace-config")
		test_create_destroy_userspace_config();

	igt_subtest("whitelisted-registers-userspace-config")
		test_whitelisted_registers_userspace_config();

	igt_subtest_group {
		igt_describe("Verify mapping of oa buffer");
		igt_subtest_with_dynamic("map-oa-buffer")
			__for_random_engine_in_each_group(perf_oa_groups, ctx, e)
				test_mapped_oa_buffer(check_reports_from_mapped_buffer, e);

		igt_describe("Verify invalid mappings of oa buffer");
		igt_subtest_with_dynamic("invalid-map-oa-buffer")
			__for_random_engine_in_each_group(perf_oa_groups, ctx, e)
				test_mapped_oa_buffer(invalid_param_map_oa_buffer, e);

		igt_describe("Verify if non-privileged user can map oa buffer");
		igt_subtest_with_dynamic("non-privileged-map-oa-buffer")
			__for_random_engine_in_each_group(perf_oa_groups, ctx, e)
				test_mapped_oa_buffer(unprivileged_map_oa_buffer, e);

		igt_describe("Verify if non-privileged user can map oa buffer");
		igt_subtest_with_dynamic("non-privileged-access-vaddr")
			__for_random_engine_in_each_group(perf_oa_groups, ctx, e)
				test_mapped_oa_buffer(map_oa_buffer_unprivilege_access, e);

		igt_describe("Verify that forked access to mapped buffer fails");
		igt_subtest_with_dynamic("privileged-forked-access-vaddr")
			__for_random_engine_in_each_group(perf_oa_groups, ctx, e)
				test_mapped_oa_buffer(map_oa_buffer_forked_access, e);

		igt_describe("Unmap buffer, close fd and try to access");
		igt_subtest_with_dynamic("closed-fd-and-unmapped-access")
			__for_random_engine_in_each_group(perf_oa_groups, ctx, e)
				closed_fd_and_unmapped_access(e);
	}

	igt_subtest_group {
		igt_fixture {
			perf_init_whitelist();
		}

		igt_describe("Verify that OA registers are whitelisted");
		igt_subtest_with_dynamic("oa-regs-whitelisted") {
			const struct intel_execution_engine2 e2 = {};

			xe_for_each_engine(drm_fd, hwe) {
				if (hwe->engine_class == DRM_XE_ENGINE_CLASS_RENDER)
					break;
			}
			igt_dynamic_f("%s", "rcs") {
				igt_require(hwe->engine_class == DRM_XE_ENGINE_CLASS_RENDER);
				test_oa_regs_whitelist(&e2, hwe);
			}

			xe_for_each_engine(drm_fd, hwe) {
				if (hwe->engine_class == DRM_XE_ENGINE_CLASS_COMPUTE)
					break;
			}
			igt_dynamic_f("%s", "ccs") {
				igt_require(hwe->engine_class == DRM_XE_ENGINE_CLASS_COMPUTE);
				test_oa_regs_whitelist(&e2, hwe);
			}
		}

		igt_describe("Verify mmio triggered reports");
		igt_subtest_with_dynamic("mmio-triggered-reports") {
			igt_require(HAS_OA_MMIO_TRIGGER(devid));

			/* Seeing engine memory cat error with this on LNL */
			if (!IS_LUNARLAKE(devid)) {
				xe_for_each_engine(drm_fd, hwe) {
					if (hwe->engine_class == DRM_XE_ENGINE_CLASS_RENDER)
						break;
				}
				igt_dynamic_f("%s", "rcs") {
					igt_require(hwe->engine_class == DRM_XE_ENGINE_CLASS_RENDER);
					test_mmio_triggered_reports(hwe);
				}
			}

			xe_for_each_engine(drm_fd, hwe) {
				if (hwe->engine_class == DRM_XE_ENGINE_CLASS_COMPUTE)
					break;
			}
			igt_dynamic_f("%s", "ccs") {
				igt_require(hwe->engine_class == DRM_XE_ENGINE_CLASS_COMPUTE);
				test_mmio_triggered_reports(hwe);
			}
		}
	}

	igt_fixture {
		/* leave sysctl options in their default state... */
		write_u64_file("/proc/sys/dev/xe/perf_stream_paranoid", 1);

		if (intel_perf)
			intel_perf_free(intel_perf);

		if (perf_oa_groups)
			put_engine_groups(perf_oa_groups, num_perf_oa_groups);

		intel_ctx_destroy(drm_fd, ctx);
		drm_close_driver(drm_fd);
	}
}
