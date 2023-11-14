// SPDX-License-Identifier: MIT
/*
 * Copyright © 2022 Intel Corporation
 */

/**
 * TEST: Check device configuration query
 * Category: Software building block
 * Sub-category: uapi
 * Functionality: ioctl
 * Description: Acquire configuration data for xe device
 */

#include <string.h>

#include "igt.h"
#include "xe_drm.h"
#include "xe/xe_ioctl.h"
#include "xe/xe_query.h"
#include "intel_hwconfig_types.h"

void dump_hex(void *buffer, int len);
void dump_hex_debug(void *buffer, int len);
const char *get_hwconfig_name(int param);
const char *get_topo_name(int value);
void process_hwconfig(void *data, uint32_t len);

void dump_hex(void *buffer, int len)
{
	unsigned char *data = (unsigned char*)buffer;
	int k = 0;
	for (int i = 0; i < len; i++) {
		igt_info(" %02x", data[i]);
		if (++k > 15) {
			k = 0;
			igt_info("\n");
		}
	}
	if (k)
		igt_info("\n");
}

void dump_hex_debug(void *buffer, int len)
{
	if (igt_log_level == IGT_LOG_DEBUG)
		dump_hex(buffer, len);
}

/* Please reflect intel_hwconfig_types.h changes below
 * static_asserti_value + get_hwconfig_name
 *   Thanks :-) */
static_assert(INTEL_HWCONFIG_MAX_MESH_URB_ENTRIES+1 == __INTEL_HWCONFIG_KEY_LIMIT, "");

#define CASE_STRINGIFY(A) case INTEL_HWCONFIG_##A: return #A;
const char* get_hwconfig_name(int param)
{
	switch(param) {
	CASE_STRINGIFY(MAX_SLICES_SUPPORTED);
	CASE_STRINGIFY(MAX_DUAL_SUBSLICES_SUPPORTED);
	CASE_STRINGIFY(MAX_NUM_EU_PER_DSS);
	CASE_STRINGIFY(NUM_PIXEL_PIPES);
	CASE_STRINGIFY(DEPRECATED_MAX_NUM_GEOMETRY_PIPES);
	CASE_STRINGIFY(DEPRECATED_L3_CACHE_SIZE_IN_KB);
	CASE_STRINGIFY(DEPRECATED_L3_BANK_COUNT);
	CASE_STRINGIFY(L3_CACHE_WAYS_SIZE_IN_BYTES);
	CASE_STRINGIFY(L3_CACHE_WAYS_PER_SECTOR);
	CASE_STRINGIFY(MAX_MEMORY_CHANNELS);
	CASE_STRINGIFY(MEMORY_TYPE);
	CASE_STRINGIFY(CACHE_TYPES);
	CASE_STRINGIFY(LOCAL_MEMORY_PAGE_SIZES_SUPPORTED);
	CASE_STRINGIFY(DEPRECATED_SLM_SIZE_IN_KB);
	CASE_STRINGIFY(NUM_THREADS_PER_EU);
	CASE_STRINGIFY(TOTAL_VS_THREADS);
	CASE_STRINGIFY(TOTAL_GS_THREADS);
	CASE_STRINGIFY(TOTAL_HS_THREADS);
	CASE_STRINGIFY(TOTAL_DS_THREADS);
	CASE_STRINGIFY(TOTAL_VS_THREADS_POCS);
	CASE_STRINGIFY(TOTAL_PS_THREADS);
	CASE_STRINGIFY(DEPRECATED_MAX_FILL_RATE);
	CASE_STRINGIFY(MAX_RCS);
	CASE_STRINGIFY(MAX_CCS);
	CASE_STRINGIFY(MAX_VCS);
	CASE_STRINGIFY(MAX_VECS);
	CASE_STRINGIFY(MAX_COPY_CS);
	CASE_STRINGIFY(DEPRECATED_URB_SIZE_IN_KB);
	CASE_STRINGIFY(MIN_VS_URB_ENTRIES);
	CASE_STRINGIFY(MAX_VS_URB_ENTRIES);
	CASE_STRINGIFY(MIN_PCS_URB_ENTRIES);
	CASE_STRINGIFY(MAX_PCS_URB_ENTRIES);
	CASE_STRINGIFY(MIN_HS_URB_ENTRIES);
	CASE_STRINGIFY(MAX_HS_URB_ENTRIES);
	CASE_STRINGIFY(MIN_GS_URB_ENTRIES);
	CASE_STRINGIFY(MAX_GS_URB_ENTRIES);
	CASE_STRINGIFY(MIN_DS_URB_ENTRIES);
	CASE_STRINGIFY(MAX_DS_URB_ENTRIES);
	CASE_STRINGIFY(PUSH_CONSTANT_URB_RESERVED_SIZE);
	CASE_STRINGIFY(POCS_PUSH_CONSTANT_URB_RESERVED_SIZE);
	CASE_STRINGIFY(URB_REGION_ALIGNMENT_SIZE_IN_BYTES);
	CASE_STRINGIFY(URB_ALLOCATION_SIZE_UNITS_IN_BYTES);
	CASE_STRINGIFY(MAX_URB_SIZE_CCS_IN_BYTES);
	CASE_STRINGIFY(VS_MIN_DEREF_BLOCK_SIZE_HANDLE_COUNT);
	CASE_STRINGIFY(DS_MIN_DEREF_BLOCK_SIZE_HANDLE_COUNT);
	CASE_STRINGIFY(NUM_RT_STACKS_PER_DSS);
	CASE_STRINGIFY(MAX_URB_STARTING_ADDRESS);
	CASE_STRINGIFY(MIN_CS_URB_ENTRIES);
	CASE_STRINGIFY(MAX_CS_URB_ENTRIES);
	CASE_STRINGIFY(L3_ALLOC_PER_BANK_URB);
	CASE_STRINGIFY(L3_ALLOC_PER_BANK_REST);
	CASE_STRINGIFY(L3_ALLOC_PER_BANK_DC);
	CASE_STRINGIFY(L3_ALLOC_PER_BANK_RO);
	CASE_STRINGIFY(L3_ALLOC_PER_BANK_Z);
	CASE_STRINGIFY(L3_ALLOC_PER_BANK_COLOR);
	CASE_STRINGIFY(L3_ALLOC_PER_BANK_UNIFIED_TILE_CACHE);
	CASE_STRINGIFY(L3_ALLOC_PER_BANK_COMMAND_BUFFER);
	CASE_STRINGIFY(L3_ALLOC_PER_BANK_RW);
	CASE_STRINGIFY(MAX_NUM_L3_CONFIGS);
	CASE_STRINGIFY(BINDLESS_SURFACE_OFFSET_BIT_COUNT);
	CASE_STRINGIFY(RESERVED_CCS_WAYS);
	CASE_STRINGIFY(CSR_SIZE_IN_MB);
	CASE_STRINGIFY(GEOMETRY_PIPES_PER_SLICE);
	CASE_STRINGIFY(L3_BANK_SIZE_IN_KB);
	CASE_STRINGIFY(SLM_SIZE_PER_DSS);
	CASE_STRINGIFY(MAX_PIXEL_FILL_RATE_PER_SLICE);
	CASE_STRINGIFY(MAX_PIXEL_FILL_RATE_PER_DSS);
	CASE_STRINGIFY(URB_SIZE_PER_SLICE_IN_KB);
	CASE_STRINGIFY(URB_SIZE_PER_L3_BANK_COUNT_IN_KB);
	CASE_STRINGIFY(MAX_SUBSLICE);
	CASE_STRINGIFY(MAX_EU_PER_SUBSLICE);
	CASE_STRINGIFY(RAMBO_L3_BANK_SIZE_IN_KB);
	CASE_STRINGIFY(SLM_SIZE_PER_SS_IN_KB);
	CASE_STRINGIFY(NUM_HBM_STACKS_PER_TILE);
	CASE_STRINGIFY(NUM_CHANNELS_PER_HBM_STACK);
	CASE_STRINGIFY(HBM_CHANNEL_WIDTH_IN_BYTES);
	CASE_STRINGIFY(MIN_TASK_URB_ENTRIES);
	CASE_STRINGIFY(MAX_TASK_URB_ENTRIES);
	CASE_STRINGIFY(MIN_MESH_URB_ENTRIES);
	CASE_STRINGIFY(MAX_MESH_URB_ENTRIES);
	}
	return "?? Please fix "__FILE__;
}
#undef CASE_STRINGIFY

void process_hwconfig(void *data, uint32_t len)
{

	uint32_t *d = (uint32_t*)data;
	uint32_t l = len / 4;
	uint32_t pos = 0;
	while (pos + 2 < l) {
		if (d[pos+1] == 1) {
			igt_info("%-37s (%3d) L:%d V: %d/0x%x\n",
				 get_hwconfig_name(d[pos]), d[pos], d[pos+1],
				 d[pos+2], d[pos+2]);
		} else {
			igt_info("%-37s (%3d) L:%d\n", get_hwconfig_name(d[pos]), d[pos], d[pos+1]);
			dump_hex(&d[pos+2], d[pos+1]);
		}
		pos += 2 + d[pos+1];
	}
}


const char *get_topo_name(int value)
{
	switch(value) {
	case DRM_XE_TOPO_DSS_GEOMETRY: return "DSS_GEOMETRY";
	case DRM_XE_TOPO_DSS_COMPUTE: return "DSS_COMPUTE";
	case DRM_XE_TOPO_EU_PER_DSS: return "EU_PER_DSS";
	}
	return "??";
}

/**
 * SUBTEST: query-engines
 * Description: Display engine classes available for xe device
 * Test category: functionality test
 */
static void
test_query_engines(int fd)
{
	struct drm_xe_engine_class_instance *hwe;
	int i = 0;

	xe_for_each_hw_engine(fd, hwe) {
		igt_assert(hwe);
		igt_info("engine %d: %s, engine instance: %d, tile: TILE-%d\n", i++,
			 xe_engine_class_string(hwe->engine_class), hwe->engine_instance,
								    hwe->gt_id);
	}

	igt_assert(i > 0);
}

/**
 * SUBTEST: query-mem-usage
 * Test category: functionality test
 * Description: Display memory information like memory class, size
 *	and alignment.
 */
static void
test_query_mem_regions(int fd)
{
	struct drm_xe_query_mem_regions *mem_regions;
	struct drm_xe_device_query query = {
		.extensions = 0,
		.query = DRM_XE_DEVICE_QUERY_MEM_REGIONS,
		.size = 0,
		.data = 0,
	};
	int i;

	igt_assert_eq(igt_ioctl(fd, DRM_IOCTL_XE_DEVICE_QUERY, &query), 0);
	igt_assert_neq(query.size, 0);

	mem_regions = malloc(query.size);
	igt_assert(mem_regions);

	query.data = to_user_pointer(mem_regions);
	igt_assert_eq(igt_ioctl(fd, DRM_IOCTL_XE_DEVICE_QUERY, &query), 0);

	for (i = 0; i < mem_regions->num_regions; i++) {
		igt_info("mem region %d: %s\t%#llx / %#llx\n", i,
			mem_regions->regions[i].mem_class ==
			DRM_XE_MEM_REGION_CLASS_SYSMEM ? "SYSMEM"
			:mem_regions->regions[i].mem_class ==
			DRM_XE_MEM_REGION_CLASS_VRAM ? "VRAM" : "?",
			mem_regions->regions[i].used,
			mem_regions->regions[i].total_size
		);
		igt_info("min_page_size=0x%x\n",
		       mem_regions->regions[i].min_page_size);

		igt_info("visible size=%lluMiB\n",
			 mem_regions->regions[i].cpu_visible_size >> 20);
		igt_info("visible used=%lluMiB\n",
			 mem_regions->regions[i].cpu_visible_used >> 20);

		igt_assert_lte_u64(mem_regions->regions[i].cpu_visible_size,
				   mem_regions->regions[i].total_size);
		igt_assert_lte_u64(mem_regions->regions[i].cpu_visible_used,
				   mem_regions->regions[i].cpu_visible_size);
		igt_assert_lte_u64(mem_regions->regions[i].cpu_visible_used,
				   mem_regions->regions[i].used);
		igt_assert_lte_u64(mem_regions->regions[i].used,
				   mem_regions->regions[i].total_size);
		igt_assert_lte_u64(mem_regions->regions[i].used -
				   mem_regions->regions[i].cpu_visible_used,
				   mem_regions->regions[i].total_size);
	}
	dump_hex_debug(mem_regions, query.size);
	free(mem_regions);
}

/**
 * SUBTEST: query-gt-list
 * Test category: functionality test
 * Description: Display information about available GT components for xe device.
 */
static void
test_query_gt_list(int fd)
{
	struct drm_xe_query_gt_list *gt_list;
	struct drm_xe_device_query query = {
		.extensions = 0,
		.query = DRM_XE_DEVICE_QUERY_GT_LIST,
		.size = 0,
		.data = 0,
	};
	int i;

	igt_assert_eq(igt_ioctl(fd, DRM_IOCTL_XE_DEVICE_QUERY, &query), 0);
	igt_assert_neq(query.size, 0);

	gt_list = malloc(query.size);
	igt_assert(gt_list);

	query.data = to_user_pointer(gt_list);
	igt_assert_eq(igt_ioctl(fd, DRM_IOCTL_XE_DEVICE_QUERY, &query), 0);

	for (i = 0; i < gt_list->num_gt; i++) {
		igt_info("type: %d\n", gt_list->gt_list[i].type);
		igt_info("gt_id: %d\n", gt_list->gt_list[i].gt_id);
		igt_info("clock_freq: %u\n", gt_list->gt_list[i].clock_freq);
		igt_info("near_mem_regions: 0x%016llx\n",
		       gt_list->gt_list[i].near_mem_regions);
		igt_info("far_mem_regions: 0x%016llx\n",
		       gt_list->gt_list[i].far_mem_regions);
	}
}

/**
 * SUBTEST: query-topology
 * Test category: functionality test
 * Description: Display topology information of GT.
 */
static void
test_query_gt_topology(int fd)
{
	struct drm_xe_query_topology_mask *topology;
	int pos = 0;
	struct drm_xe_device_query query = {
		.extensions = 0,
		.query = DRM_XE_DEVICE_QUERY_GT_TOPOLOGY,
		.size = 0,
		.data = 0,
	};

	igt_assert_eq(igt_ioctl(fd, DRM_IOCTL_XE_DEVICE_QUERY, &query), 0);
	igt_assert_neq(query.size, 0);

	topology = malloc(query.size);
	igt_assert(topology);

	query.data = to_user_pointer(topology);
	igt_assert_eq(igt_ioctl(fd, DRM_IOCTL_XE_DEVICE_QUERY, &query), 0);

	igt_info("size: %d\n", query.size);
	dump_hex_debug(topology, query.size);

	while (query.size >= sizeof(struct drm_xe_query_topology_mask)) {
		struct drm_xe_query_topology_mask *topo = (struct drm_xe_query_topology_mask*)((unsigned char*)topology + pos);
		int sz = sizeof(struct drm_xe_query_topology_mask) + topo->num_bytes;
		igt_info(" gt_id: %2d type: %-12s (%d) n:%d [%d] ", topo->gt_id,
			 get_topo_name(topo->type), topo->type, topo->num_bytes, sz);
		for (int j=0; j< topo->num_bytes; j++)
			igt_info(" %02x", topo->mask[j]);
		igt_info("\n");
		query.size -= sz;
		pos += sz;
	}

	free(topology);
}

/**
 * SUBTEST: query-config
 * Test category: functionality test
 * Description: Display xe device id, revision and configuration.
 */
static void
test_query_config(int fd)
{
	struct drm_xe_query_config *config;
	struct drm_xe_device_query query = {
		.extensions = 0,
		.query = DRM_XE_DEVICE_QUERY_CONFIG,
		.size = 0,
		.data = 0,
	};

	igt_assert_eq(igt_ioctl(fd, DRM_IOCTL_XE_DEVICE_QUERY, &query), 0);
	igt_assert_neq(query.size, 0);

	config = malloc(query.size);
	igt_assert(config);

	query.data = to_user_pointer(config);
	igt_assert_eq(igt_ioctl(fd, DRM_IOCTL_XE_DEVICE_QUERY, &query), 0);

	igt_assert(config->num_params > 0);

	igt_info("DRM_XE_QUERY_CONFIG_REV_AND_DEVICE_ID\t%#llx\n",
		config->info[DRM_XE_QUERY_CONFIG_REV_AND_DEVICE_ID]);
	igt_info("  REV_ID\t\t\t\t%#llx\n",
		config->info[DRM_XE_QUERY_CONFIG_REV_AND_DEVICE_ID] >> 16);
	igt_info("  DEVICE_ID\t\t\t\t%#llx\n",
		config->info[DRM_XE_QUERY_CONFIG_REV_AND_DEVICE_ID] & 0xffff);
	igt_info("DRM_XE_QUERY_CONFIG_FLAGS\t\t\t%#llx\n",
		config->info[DRM_XE_QUERY_CONFIG_FLAGS]);
	igt_info("  DRM_XE_QUERY_CONFIG_FLAG_HAS_VRAM\t%s\n",
		config->info[DRM_XE_QUERY_CONFIG_FLAGS] &
		DRM_XE_QUERY_CONFIG_FLAG_HAS_VRAM ? "ON":"OFF");
	igt_info("DRM_XE_QUERY_CONFIG_MIN_ALIGNMENT\t\t%#llx\n",
		config->info[DRM_XE_QUERY_CONFIG_MIN_ALIGNMENT]);
	igt_info("DRM_XE_QUERY_CONFIG_VA_BITS\t\t\t%llu\n",
		config->info[DRM_XE_QUERY_CONFIG_VA_BITS]);
	igt_info("DRM_XE_QUERY_CONFIG_MAX_EXEC_QUEUE_PRIORITY\t%llu\n",
		config->info[DRM_XE_QUERY_CONFIG_MAX_EXEC_QUEUE_PRIORITY]);
	dump_hex_debug(config, query.size);

	free(config);
}

/**
 * SUBTEST: query-hwconfig
 * Test category: functionality test
 * Description: Display hardware configuration of xe device.
 */
static void
test_query_hwconfig(int fd)
{
	void *hwconfig;
	struct drm_xe_device_query query = {
		.extensions = 0,
		.query = DRM_XE_DEVICE_QUERY_HWCONFIG,
		.size = 0,
		.data = 0,
	};

	igt_assert_eq(igt_ioctl(fd, DRM_IOCTL_XE_DEVICE_QUERY, &query), 0);

	igt_info("HWCONFIG_SIZE\t%u\n", query.size);
	if (!query.size)
		return;

	hwconfig = malloc(query.size);
	igt_assert(hwconfig);

	query.data = to_user_pointer(hwconfig);
	igt_assert_eq(igt_ioctl(fd, DRM_IOCTL_XE_DEVICE_QUERY, &query), 0);

	dump_hex_debug(hwconfig, query.size);
	process_hwconfig(hwconfig, query.size);

	free(hwconfig);
}

/**
 * SUBTEST: query-invalid-query
 * Test category: negative test
 * Description: Check query with invalid arguments returns expected error code.
 */
static void
test_query_invalid_query(int fd)
{
	struct drm_xe_device_query query = {
		.extensions = 0,
		.query = UINT32_MAX,
		.size = 0,
		.data = 0,
	};

	do_ioctl_err(fd, DRM_IOCTL_XE_DEVICE_QUERY, &query, EINVAL);
}

/**
 * SUBTEST: query-invalid-size
 * Test category: negative test
 * Description: Check query with invalid size returns expected error code.
 */
static void
test_query_invalid_size(int fd)
{
	struct drm_xe_device_query query = {
		.extensions = 0,
		.query = DRM_XE_DEVICE_QUERY_CONFIG,
		.size = UINT32_MAX,
		.data = 0,
	};

	do_ioctl_err(fd, DRM_IOCTL_XE_DEVICE_QUERY, &query, EINVAL);
}

/**
 * SUBTEST: query-invalid-extension
 * Test category: negative test
 * Description: Check query with invalid extension returns expected error code.
 */
static void
test_query_invalid_extension(int fd)
{
	struct drm_xe_device_query query = {
		.extensions = -1,
		.query = DRM_XE_DEVICE_QUERY_CONFIG,
		.size = 0,
		.data = 0,
	};

	do_ioctl_err(fd, DRM_IOCTL_XE_DEVICE_QUERY, &query, EINVAL);
}

static bool
query_engine_cycles_supported(int fd)
{
	struct drm_xe_device_query query = {
		.extensions = 0,
		.query = DRM_XE_DEVICE_QUERY_ENGINE_CYCLES,
		.size = 0,
		.data = 0,
	};

	return igt_ioctl(fd, DRM_IOCTL_XE_DEVICE_QUERY, &query) == 0;
}

static void
query_engine_cycles(int fd, struct drm_xe_query_engine_cycles *resp)
{
	struct drm_xe_device_query query = {
		.extensions = 0,
		.query = DRM_XE_DEVICE_QUERY_ENGINE_CYCLES,
		.size = sizeof(*resp),
		.data = to_user_pointer(resp),
	};

	do_ioctl(fd, DRM_IOCTL_XE_DEVICE_QUERY, &query);
	igt_assert(query.size);
}

static void
__engine_cycles(int fd, struct drm_xe_engine_class_instance *hwe)
{
	struct drm_xe_query_engine_cycles ts1 = {};
	struct drm_xe_query_engine_cycles ts2 = {};
	uint64_t delta_cpu, delta_cs, delta_delta;
	unsigned int exec_queue;
	int i, usable = 0;
	igt_spin_t *spin;
	uint64_t ahnd;
	uint32_t vm;
	struct {
		int32_t id;
		const char *name;
	} clock[] = {
		{ CLOCK_MONOTONIC, "CLOCK_MONOTONIC" },
		{ CLOCK_MONOTONIC_RAW, "CLOCK_MONOTONIC_RAW" },
		{ CLOCK_REALTIME, "CLOCK_REALTIME" },
		{ CLOCK_BOOTTIME, "CLOCK_BOOTTIME" },
		{ CLOCK_TAI, "CLOCK_TAI" },
	};

	igt_debug("engine[%u:%u]\n",
		  hwe->engine_class,
		  hwe->engine_instance);

	vm = xe_vm_create(fd, 0, 0);
	exec_queue = xe_exec_queue_create(fd, vm, hwe, 0);
	ahnd = intel_allocator_open(fd, 0, INTEL_ALLOCATOR_RELOC);
	spin = igt_spin_new(fd, .ahnd = ahnd, .engine = exec_queue, .vm = vm);

	/* Try a new clock every 10 iterations. */
#define NUM_SNAPSHOTS 10
	for (i = 0; i < NUM_SNAPSHOTS * ARRAY_SIZE(clock); i++) {
		int index = i / NUM_SNAPSHOTS;

		ts1.eci = *hwe;
		ts1.clockid = clock[index].id;

		ts2.eci = *hwe;
		ts2.clockid = clock[index].id;

		query_engine_cycles(fd, &ts1);
		query_engine_cycles(fd, &ts2);

		igt_debug("[1] cpu_ts before %llu, reg read time %llu\n",
			  ts1.cpu_timestamp,
			  ts1.cpu_delta);
		igt_debug("[1] engine_ts %llu, freq %llu Hz, width %u\n",
			  ts1.engine_cycles, ts1.engine_frequency, ts1.width);

		igt_debug("[2] cpu_ts before %llu, reg read time %llu\n",
			  ts2.cpu_timestamp,
			  ts2.cpu_delta);
		igt_debug("[2] engine_ts %llu, freq %llu Hz, width %u\n",
			  ts2.engine_cycles, ts2.engine_frequency, ts2.width);

		delta_cpu = ts2.cpu_timestamp - ts1.cpu_timestamp;

		if (ts2.engine_cycles >= ts1.engine_cycles)
			delta_cs = (ts2.engine_cycles - ts1.engine_cycles) *
				   NSEC_PER_SEC / ts1.engine_frequency;
		else
			delta_cs = (((1 << ts2.width) - ts2.engine_cycles) + ts1.engine_cycles) *
				   NSEC_PER_SEC / ts1.engine_frequency;

		igt_debug("delta_cpu[%lu], delta_cs[%lu]\n",
			  delta_cpu, delta_cs);

		delta_delta = delta_cpu > delta_cs ?
			       delta_cpu - delta_cs :
			       delta_cs - delta_cpu;
		igt_debug("delta_delta %lu\n", delta_delta);

		if (delta_delta < 5000)
			usable++;

		/*
		 * User needs few good snapshots of the timestamps to
		 * synchronize cpu time with cs time. Check if we have enough
		 * usable values before moving to the next clockid.
		 */
		if (!((i + 1) % NUM_SNAPSHOTS)) {
			igt_debug("clock %s\n", clock[index].name);
			igt_debug("usable %d\n", usable);
			igt_assert(usable > 2);
			usable = 0;
		}
	}

	igt_spin_free(fd, spin);
	xe_exec_queue_destroy(fd, exec_queue);
	xe_vm_destroy(fd, vm);
	put_ahnd(ahnd);
}

/**
 * SUBTEST: query-cs-cycles
 * Description: Query CPU-GPU timestamp correlation
 */
static void test_query_engine_cycles(int fd)
{
	struct drm_xe_engine_class_instance *hwe;

	igt_require(query_engine_cycles_supported(fd));

	xe_for_each_hw_engine(fd, hwe) {
		igt_assert(hwe);
		__engine_cycles(fd, hwe);
	}
}

/**
 * SUBTEST: query-invalid-cs-cycles
 * Description: Check query with invalid arguments returns expected error code.
 */
static void test_engine_cycles_invalid(int fd)
{
	struct drm_xe_engine_class_instance *hwe;
	struct drm_xe_query_engine_cycles ts = {};
	struct drm_xe_device_query query = {
		.extensions = 0,
		.query = DRM_XE_DEVICE_QUERY_ENGINE_CYCLES,
		.size = sizeof(ts),
		.data = to_user_pointer(&ts),
	};

	igt_require(query_engine_cycles_supported(fd));

	/* get one engine */
	xe_for_each_hw_engine(fd, hwe)
		break;

	/* sanity check engine selection is valid */
	ts.eci = *hwe;
	query_engine_cycles(fd, &ts);

	/* bad instance */
	ts.eci = *hwe;
	ts.eci.engine_instance = 0xffff;
	do_ioctl_err(fd, DRM_IOCTL_XE_DEVICE_QUERY, &query, EINVAL);
	ts.eci = *hwe;

	/* bad class */
	ts.eci.engine_class = 0xffff;
	do_ioctl_err(fd, DRM_IOCTL_XE_DEVICE_QUERY, &query, EINVAL);
	ts.eci = *hwe;

	/* bad gt */
	ts.eci.gt_id = 0xffff;
	do_ioctl_err(fd, DRM_IOCTL_XE_DEVICE_QUERY, &query, EINVAL);
	ts.eci = *hwe;

	/* bad clockid */
	ts.clockid = -1;
	do_ioctl_err(fd, DRM_IOCTL_XE_DEVICE_QUERY, &query, EINVAL);
	ts.clockid = 0;

	/* sanity check */
	query_engine_cycles(fd, &ts);
}

igt_main
{
	int xe;

	igt_fixture
		xe = drm_open_driver(DRIVER_XE);

	igt_subtest("query-engines")
		test_query_engines(xe);

	igt_subtest("query-mem-usage")
		test_query_mem_regions(xe);

	igt_subtest("query-gt-list")
		test_query_gt_list(xe);

	igt_subtest("query-config")
		test_query_config(xe);

	igt_subtest("query-hwconfig")
		test_query_hwconfig(xe);

	igt_subtest("query-topology")
		test_query_gt_topology(xe);

	igt_subtest("query-cs-cycles")
		test_query_engine_cycles(xe);

	igt_subtest("query-invalid-cs-cycles")
		test_engine_cycles_invalid(xe);

	igt_subtest("query-invalid-query")
		test_query_invalid_query(xe);

	igt_subtest("query-invalid-size")
		test_query_invalid_size(xe);

	igt_subtest("query-invalid-extension")
		test_query_invalid_extension(xe);

	igt_fixture
		drm_close_driver(xe);
}
