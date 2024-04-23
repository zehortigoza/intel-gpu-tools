// SPDX-License-Identifier: MIT
/*
 * Copyright © 2021 Intel Corporation
 */

/**
 * TEST: Check compression functionality
 * Category: Software building block
 * Sub-category: compression
 */

#include "igt.h"
#include "xe_drm.h"
#include "xe/xe_ioctl.h"
#include "xe/xe_query.h"
#include "intel_pat.h"
#include "intel_mocs.h"
#include "lib/igt_syncobj.h"

#define WIDTH 800
#define HEIGHT 600
#define NUM_DWORDS (WIDTH * HEIGHT)

#define ADDR_BO_CPU 	     0x1000000
#define ADDR_BO_COMPRESSED   0x2000000
#define ADDR_BO_UNCOMPRESSED 0x3000000
#define ADDR_BO_BATCH        0x4000000

static bool debug_enabled = false;

struct data {
	int fd;
	uint32_t vm_id;
	uint32_t exec_queue;

	uint32_t bo_cpu;
	uint32_t bo_compressed;
	uint32_t *bo_cpu_map;

	struct buf_ops *bops;
	struct intel_buf buf_cpu;
	struct intel_buf buf_compressed;
};

static uint32_t
addr_low(uint64_t addr)
{
	return addr;
}

static uint32_t
addr_high(int fd, uint64_t addr)
{
	uint32_t va_bits = xe_va_bits(fd);
	uint32_t leading_bits = 64 - va_bits;

	igt_assert_eq(addr >> va_bits, 0);
	return (int64_t)(addr << leading_bits) >> (32 + leading_bits);
}

static void
copy_dwords_with_flush(struct data *data, uint64_t src, uint64_t dest,
		       uint32_t num, bool flush_before)
{
	uint16_t dev_id = intel_get_drm_devid(data->fd);
	const struct intel_device_info *device_info = intel_get_device_info(dev_id);
	uint32_t bo_batch, *batch_map, batch_index = 0, i;
	uint64_t batch_size;

	batch_size = (NUM_DWORDS * 5 + 1) * sizeof(uint32_t);
	if (flush_before)
		batch_size += 4 * sizeof(uint32_t);
	batch_size = xe_bb_size(data->fd, batch_size);
	bo_batch = xe_bo_create(data->fd, data->vm_id, batch_size,
				vram_if_possible(data->fd, 0),
				DRM_XE_GEM_CREATE_FLAG_NEEDS_VISIBLE_VRAM);
	batch_map = xe_bo_mmap_ext(data->fd, bo_batch, batch_size,
				   PROT_READ | PROT_WRITE);
	xe_vm_bind_sync(data->fd, data->vm_id, bo_batch, 0, ADDR_BO_BATCH, batch_size);

	if (flush_before && device_info->graphics_ver >= 20) {
		batch_map[batch_index++] = GFX_OP_PIPE_CONTROL(2) |
					   PIPE_CONTROL0_CCS_FLUSH |
					   REG_BIT(11) | /* Untyped Data-Port Cache Flush */
					   REG_BIT(9); /* Dataport Flush */
		batch_map[batch_index++] = PIPE_CONTROL_CS_STALL |
					   REG_BIT(18) | /* TLB Invalidate */
					   REG_BIT(5) | /* DC Flush Enable */
					   REG_BIT(2);/* State Cache Invalidation Enable */
		if (debug_enabled) {
			printf("PIPE_CONTROL: 0x%x 0x%x\n",
			       batch_map[batch_index - 2],
			       batch_map[batch_index - 1]);
		}

	}

	for (i = 0; i < NUM_DWORDS; i++) {
		uint64_t dest_i = dest + i * sizeof(uint32_t);
		uint64_t src_i = src + i * sizeof(uint32_t);

		batch_map[batch_index++] = MI_COPY_MEM_MEM;
		batch_map[batch_index++] = addr_low(dest_i);
		batch_map[batch_index++] = addr_high(data->fd, dest_i);
		batch_map[batch_index++] = addr_low(src_i);
		batch_map[batch_index++] = addr_high(data->fd, src_i);
	}
	batch_map[batch_index++] = MI_BATCH_BUFFER_END;
	igt_assert(batch_size >= batch_index);
	xe_exec_wait(data->fd, data->exec_queue, ADDR_BO_BATCH);

	munmap(batch_map, batch_size);
	xe_vm_unbind_sync(data->fd, data->vm_id, 0, ADDR_BO_BATCH, batch_size);
	gem_close(data->fd, bo_batch);
}

static void
copy_dwords(struct data *data, uint64_t src, uint64_t dest, uint32_t num)
{
	copy_dwords_with_flush(data, src, dest, num, false);
}

static void
vma_bind(struct data *data, uint32_t bo, uint64_t addr, uint32_t size, uint8_t pat_index)
{
	struct drm_xe_sync sync = {
		.type = DRM_XE_SYNC_TYPE_SYNCOBJ,
		.flags = DRM_XE_SYNC_FLAG_SIGNAL,
		.handle = syncobj_create(data->fd, 0),
	};
	int ret;

	ret = __xe_vm_bind(data->fd, data->vm_id, 0, bo, 0, addr, size,
			   DRM_XE_VM_BIND_OP_MAP, 0, &sync, 1, 0, pat_index, 0);
	igt_assert(ret == 0);

	igt_assert(syncobj_wait(data->fd, &sync.handle, 1, INT64_MAX, 0, NULL));
	syncobj_destroy(data->fd, sync.handle);
}

static uint64_t
calc_bo_size(struct data *data)
{
	uint64_t bo_size = NUM_DWORDS * sizeof(uint32_t);

	return ALIGN(bo_size, xe_get_default_alignment(data->fd));
}

/*
 * intel_pat is returnig wrong indexes for Xe2 so hard-coding indexes.
 * intel_get_pat_idx_wt() is returning a compressed WT index
 * intel_get_pat_idx_uc_comp() Xe uAPI only accept WC/WT or WB, there no UC.
 */
static uint8_t
get_wc_uncompressed_pat(struct data *data)
{
	uint16_t dev_id = intel_get_drm_devid(data->fd);

	if (intel_get_device_info(dev_id)->graphics_ver == 20)
		return 6;
	return intel_get_pat_idx_wt(data->fd);
}

static uint8_t
get_wc_compressed_pat(struct data *data)
{
	uint16_t dev_id = intel_get_drm_devid(data->fd);

	if (intel_get_device_info(dev_id)->graphics_ver == 20)
		return 11;
	return intel_get_pat_idx_wt(data->fd);
}

static void
prepare(struct data *data)
{
	uint64_t bo_size = calc_bo_size(data);
	uint8_t wc_uncompressed_pat, wc_compressed_pat;
	uint32_t ret;

	wc_uncompressed_pat = get_wc_uncompressed_pat(data);
	wc_compressed_pat = get_wc_compressed_pat(data);

	ret = __xe_bo_create_caching(data->fd, data->vm_id, bo_size,
				     vram_if_possible(data->fd, 0),
				     DRM_XE_GEM_CREATE_FLAG_NEEDS_VISIBLE_VRAM,
				     DRM_XE_GEM_CPU_CACHING_WC, &data->bo_cpu);
	igt_assert(ret == 0);
	vma_bind(data, data->bo_cpu, ADDR_BO_CPU, bo_size, wc_uncompressed_pat);
	data->bo_cpu_map = xe_bo_mmap_ext(data->fd, data->bo_cpu, bo_size,
					  PROT_READ | PROT_WRITE);

	ret = __xe_bo_create_caching(data->fd, data->vm_id, bo_size,
				     vram_if_possible(data->fd, 0),
				     0,
				     DRM_XE_GEM_CPU_CACHING_WC, &data->bo_compressed);
	igt_assert(ret == 0);
	vma_bind(data, data->bo_compressed, ADDR_BO_COMPRESSED, bo_size, wc_compressed_pat);
	vma_bind(data, data->bo_compressed, ADDR_BO_UNCOMPRESSED, bo_size, wc_uncompressed_pat);
}

static void
finish(struct data *data)
{
	uint64_t bo_size = calc_bo_size(data);

	munmap(data->bo_cpu_map, bo_size);
	xe_vm_unbind_sync(data->fd, data->vm_id, 0, ADDR_BO_BATCH, bo_size);
	xe_vm_unbind_sync(data->fd, data->vm_id, 0, ADDR_BO_COMPRESSED, bo_size);
	xe_vm_unbind_sync(data->fd, data->vm_id, 0, ADDR_BO_UNCOMPRESSED, bo_size);
	gem_close(data->fd, data->bo_cpu);
	gem_close(data->fd, data->bo_compressed);
}

static void
write_cpu_map(struct data *data)
{
	uint32_t i;

	for (i = 0; i < NUM_DWORDS; i++) {
		uint32_t val = 0xc0ffee;

		if ((i % 10) == 0)
			val = i;
		data->bo_cpu_map[i] = val;
	}
}

static void
check_cpu_map(struct data *data)
{
	uint32_t i;

	for (i = 0; i < NUM_DWORDS; i++) {
		uint32_t expected = 0xc0ffee;

		if (i % 10 == 0)
			expected = i;

		if (debug_enabled)
			printf("i=%i value=%i\n", i, data->bo_cpu_map[i]);
		igt_assert(data->bo_cpu_map[i] == expected);
	}
}

static void
prepare_with_buf(struct data *data)
{
	uint32_t bpp = 32;
	uint32_t alignment = 0;
	uint32_t req_tiling = 0;
	uint32_t compression = 0;
	uint32_t size = calc_bo_size(data);
	int stride = 0;
	uint64_t region = system_memory(data->fd);
	uint8_t wc_uncompressed_pat = get_wc_uncompressed_pat(data);
	uint8_t wc_compressed_pat = get_wc_uncompressed_pat(data);

	prepare(data);

	intel_buf_init_full(data->bops, data->bo_cpu, &data->buf_cpu,
			    WIDTH, HEIGHT, bpp, alignment, req_tiling,
			    compression, size, stride, region, wc_uncompressed_pat,
			    DEFAULT_MOCS_INDEX);

	intel_buf_init_full(data->bops, data->bo_compressed, &data->buf_compressed,
			    WIDTH, HEIGHT, bpp, alignment, req_tiling,
			    compression, size, stride, region, wc_compressed_pat,
			    DEFAULT_MOCS_INDEX);
}

/**
 * SUBTEST: basic-render-copy
 * Description: Basic compression test
 * Functionality: Test basic compression read and write
 * Test category: functionality test
 */

static void
basic_render_copy(struct data *data)
{
	uint16_t dev_id = intel_get_drm_devid(data->fd);
	igt_render_copyfunc_t rendercopy = igt_get_render_copyfunc(dev_id);
	uint32_t white = 0xFFFFFFFF;
	uint32_t different_color = 0xFF00FFFF;
	struct intel_bb *ibb;
	uint32_t *expected = malloc(NUM_DWORDS * sizeof(uint32_t));
	uint32_t i;

	prepare_with_buf(data);

	/* draw white screen with a rectangle in the middle */
	igt_draw_rect(data->fd, data->bops, 0, data->buf_cpu.handle,
		      data->buf_cpu.bo_size, data->buf_cpu.surface[0].stride,
		      data->buf_cpu.width, data->buf_cpu.height,
		      data->buf_cpu.tiling, IGT_DRAW_MMAP_WC, 0, 0, WIDTH,
		      HEIGHT, white, 32);
	igt_draw_rect(data->fd, data->bops, 0, data->buf_cpu.handle,
		      data->buf_cpu.bo_size, data->buf_cpu.surface[0].stride,
		      data->buf_cpu.width, data->buf_cpu.height,
		      data->buf_cpu.tiling, IGT_DRAW_MMAP_WC, 0, 100, WIDTH,
		      200, different_color, 32);
	/* copy it to a buffer that will be compared at the end of the test */
	memcpy(expected, data->bo_cpu_map, NUM_DWORDS * sizeof(uint32_t));

	/* copy cpu buffer to compressed using GPU */
	ibb = intel_bb_create_with_context(data->fd, data->exec_queue,
					   data->vm_id, NULL, 0x1000);
	rendercopy(ibb, &data->buf_cpu, 0, 0, WIDTH, HEIGHT,
		   &data->buf_compressed, 0, 0);
	intel_bb_destroy(ibb);

	/* set CPU buffer to 0 */
	memset(data->bo_cpu_map, 0, NUM_DWORDS * sizeof(uint32_t));

	/* copy compressed buffer to CPU buffer using GPU */
	ibb = intel_bb_create_with_context(data->fd, data->exec_queue,
					   data->vm_id, NULL, 0x1000);
	rendercopy(ibb, &data->buf_compressed, 0, 0, WIDTH, HEIGHT,
		   &data->buf_cpu, 0, 0);
	intel_bb_destroy(ibb);

	/* check if passed */
	for (i = 0; i < NUM_DWORDS; i++) {
		if (debug_enabled && expected[i] != data->bo_cpu_map[i])
			printf("i=%i value=%u expected=%u\n", i, data->bo_cpu_map[i], expected[i]);
		igt_assert(expected[i] == data->bo_cpu_map[i]);
	}

	free(expected);
	finish(data);
}

/**
 * SUBTEST: basic
 * Description: Basic compression test
 * Functionality: Test basic compression read and write
 * Test category: functionality test
 */

static void
basic(struct data *data)
{
	prepare(data);

	write_cpu_map(data);
	copy_dwords(data, ADDR_BO_CPU, ADDR_BO_COMPRESSED, NUM_DWORDS);
	memset(data->bo_cpu_map, 0, NUM_DWORDS * sizeof(uint32_t));
	copy_dwords(data, ADDR_BO_COMPRESSED, ADDR_BO_CPU, NUM_DWORDS);
	check_cpu_map(data);

	finish(data);
}

/**
 * SUBTEST: resolve-compressed-to-uncompressed
 * Description: Test resolve pass from compressed to uncompressed buffer
 * Functionality: compression
 * Test category: functionality test
 */

static void
resolve_compressed_to_uncompressed(struct data *data)
{
	prepare(data);

	write_cpu_map(data);
	copy_dwords(data, ADDR_BO_CPU, ADDR_BO_COMPRESSED, NUM_DWORDS);
	memset(data->bo_cpu_map, 0, NUM_DWORDS * sizeof(uint32_t));
	copy_dwords_with_flush(data, ADDR_BO_COMPRESSED, ADDR_BO_UNCOMPRESSED, NUM_DWORDS, true);
	copy_dwords(data, ADDR_BO_UNCOMPRESSED, ADDR_BO_CPU, NUM_DWORDS);
	check_cpu_map(data);

	finish(data);
}

/**
 * SUBTEST: resolve-uncompressed-to-compressed
 * Description: Test resolve pass from uncompressed to compressed buffer
 * Functionality: compression
 * Test category: functionality test
 */

static void
resolve_uncompressed_to_compressed(struct data *data)
{
	prepare(data);

	write_cpu_map(data);
	copy_dwords_with_flush(data, ADDR_BO_CPU, ADDR_BO_UNCOMPRESSED, NUM_DWORDS, true);
	memset(data->bo_cpu_map, 0, NUM_DWORDS * sizeof(uint32_t));
	copy_dwords_with_flush(data, ADDR_BO_UNCOMPRESSED, ADDR_BO_COMPRESSED, NUM_DWORDS, true);
	copy_dwords_with_flush(data, ADDR_BO_COMPRESSED, ADDR_BO_CPU, NUM_DWORDS, true);
	check_cpu_map(data);

	finish(data);
}

static const char help_str[] = "  --print-result		Pring result buffer\n";

static int opt_handler(int option, int option_index, void *data)
{
	switch (option) {
	case 'p':
		debug_enabled = true;
		break;
	default:
		return IGT_OPT_HANDLER_ERROR;
	}

	return IGT_OPT_HANDLER_SUCCESS;
}

static const struct option long_options[] = {
	{ "print-result", 0, 0, 'p' },
	{}
};

igt_main_args("", long_options, help_str, opt_handler, NULL)
{
	struct data data = {};

	igt_fixture {
		data.fd = drm_open_driver(DRIVER_XE);
		data.vm_id = xe_vm_create(data.fd, DRM_XE_VM_CREATE_FLAG_SCRATCH_PAGE, 0);
		data.exec_queue = xe_exec_queue_create_class(data.fd, data.vm_id, DRM_XE_ENGINE_CLASS_RENDER);
		data.bops = buf_ops_create(data.fd);
	}

	igt_subtest("basic")
		basic(&data);

	igt_subtest("basic-render-copy")
		basic_render_copy(&data);

	igt_subtest("resolve-compressed-to-uncompressed")
		resolve_compressed_to_uncompressed(&data);

	igt_subtest("resolve-uncompressed-to-compressed")
		resolve_uncompressed_to_compressed(&data);

	igt_fixture {
		buf_ops_destroy(data.bops);
		xe_exec_queue_destroy(data.fd, data.exec_queue);
		xe_vm_destroy(data.fd, data.vm_id);
		drm_close_driver(data.fd);
	}
}