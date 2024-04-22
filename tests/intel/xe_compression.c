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
	uint32_t render_exec_queue;
	uint32_t copy_exec_queue;

	uint64_t buffer_size;

	uint32_t bo_cpu;
	uint32_t *bo_cpu_map;

	uint32_t bo_compressed;

	uint32_t bo_batch_buffer;
	uint64_t batch_bufer_size;

	struct buf_ops *bops;
	struct intel_buf buf_cpu;
	struct intel_buf buf_compressed;
	struct intel_buf buf_uncompressed;
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

/* do copy with MI_COPY_MEM_MEM */
static void
mi_copy_dwords_with_flush(struct data *data, uint64_t src, uint64_t dest,
		          uint32_t num, bool flush_before)
{
	uint16_t dev_id = intel_get_drm_devid(data->fd);
	const struct intel_device_info *device_info = intel_get_device_info(dev_id);
	uint32_t *batch_map, batch_index = 0, i;

	batch_map = xe_bo_mmap_ext(data->fd, data->bo_batch_buffer,
				   data->batch_bufer_size, PROT_READ | PROT_WRITE);

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
	igt_assert(data->batch_bufer_size >= batch_index);
	munmap(batch_map, data->batch_bufer_size);
	xe_exec_wait(data->fd, data->render_exec_queue, ADDR_BO_BATCH);
}

static void
mi_copy_dwords(struct data *data, uint64_t src, uint64_t dest, uint32_t num)
{
	mi_copy_dwords_with_flush(data, src, dest, num, false);
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
			   DRM_XE_VM_BIND_OP_MAP, DRM_XE_VM_BIND_FLAG_DUMPABLE,
			   &sync, 1, 0, pat_index, 0);
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
	uint8_t wc_uncompressed_pat, wc_compressed_pat;
	uint32_t ret;

	data->buffer_size = calc_bo_size(data);
	wc_uncompressed_pat = get_wc_uncompressed_pat(data);
	wc_compressed_pat = get_wc_compressed_pat(data);

	ret = __xe_bo_create_caching(data->fd, data->vm_id, data->buffer_size,
				     vram_if_possible(data->fd, 0),
				     DRM_XE_GEM_CREATE_FLAG_NEEDS_VISIBLE_VRAM,
				     DRM_XE_GEM_CPU_CACHING_WC, &data->bo_cpu);
	igt_assert(ret == 0);
	vma_bind(data, data->bo_cpu, ADDR_BO_CPU, data->buffer_size, wc_uncompressed_pat);
	data->bo_cpu_map = xe_bo_mmap_ext(data->fd, data->bo_cpu, data->buffer_size,
					  PROT_READ | PROT_WRITE);

	ret = __xe_bo_create_caching(data->fd, data->vm_id, data->buffer_size,
				     vram_if_possible(data->fd, 0),
				     0,
				     DRM_XE_GEM_CPU_CACHING_WC, &data->bo_compressed);
	igt_assert(ret == 0);
	vma_bind(data, data->bo_compressed, ADDR_BO_COMPRESSED, data->buffer_size, wc_compressed_pat);
	vma_bind(data, data->bo_compressed, ADDR_BO_UNCOMPRESSED, data->buffer_size, wc_uncompressed_pat);

	data->batch_bufer_size = (NUM_DWORDS * 5 + 1) * sizeof(uint32_t);
	data->batch_bufer_size += 4 * sizeof(uint32_t);
	data->batch_bufer_size = xe_bb_size(data->fd, data->batch_bufer_size);
	ret = __xe_bo_create_caching(data->fd, data->vm_id, data->batch_bufer_size,
				     vram_if_possible(data->fd, 0),
				     DRM_XE_GEM_CREATE_FLAG_NEEDS_VISIBLE_VRAM,
				     DRM_XE_GEM_CPU_CACHING_WC, &data->bo_batch_buffer);
	igt_assert(ret == 0);
	vma_bind(data, data->bo_batch_buffer, ADDR_BO_BATCH, data->batch_bufer_size,
		 wc_uncompressed_pat);
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
write_coffee_cpu_map(struct data *data)
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
check_coffee_cpu_map(struct data *data)
{
	uint32_t i;

	for (i = 0; i < NUM_DWORDS; i++) {
		uint32_t expected = 0xc0ffee;

		if (i % 10 == 0)
			expected = i;

		if (expected != data->bo_cpu_map[i])
			igt_warn("i=%i value=0x%x expected=0x%x\n", i, data->bo_cpu_map[i], expected);
		igt_assert(data->bo_cpu_map[i] == expected);
	}
}

static void
prepare_with_buf(struct data *data)
{
	const uint32_t bpp = 32;
	const uint32_t alignment = 0;
	const uint32_t req_tiling = 0;
	const uint32_t compression = 0;
	const uint32_t size = calc_bo_size(data);
	const int stride = 0;
	const uint64_t region = system_memory(data->fd);
	uint8_t wc_uncompressed_pat = get_wc_uncompressed_pat(data);
	uint8_t wc_compressed_pat = get_wc_uncompressed_pat(data);

	prepare(data);

	intel_buf_init_full(data->bops, data->bo_cpu, &data->buf_cpu,
			    WIDTH, HEIGHT, bpp, alignment, req_tiling,
			    compression, size, stride, region, wc_uncompressed_pat,
			    DEFAULT_MOCS_INDEX);
	data->buf_cpu.addr.offset = ADDR_BO_CPU;

	intel_buf_init_full(data->bops, data->bo_compressed, &data->buf_compressed,
			    WIDTH, HEIGHT, bpp, alignment, req_tiling,
			    compression, size, stride, region, wc_compressed_pat,
			    DEFAULT_MOCS_INDEX);
	data->buf_compressed.addr.offset = ADDR_BO_COMPRESSED;

	intel_buf_init_full(data->bops, data->bo_compressed, &data->buf_uncompressed,
			    WIDTH, HEIGHT, bpp, alignment, req_tiling,
			    compression, size, stride, region, wc_uncompressed_pat,
			    DEFAULT_MOCS_INDEX);
	data->buf_uncompressed.addr.offset = ADDR_BO_UNCOMPRESSED;
}

static void
draw_rectangle_to_cpu_bo(struct data *data, uint32_t *expected)
{
	const uint32_t white = 0xFFFFFFFF;
	const uint32_t different_color = 0xFF00FFFF;

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
}

static void
verify(struct data *data, uint32_t *expected)
{
	uint32_t i;

	/* check if passed */
	for (i = 0; i < NUM_DWORDS; i++) {
		if (expected[i] != data->bo_cpu_map[i])
			igt_warn("i=%i value=%u expected=%u\n", i, data->bo_cpu_map[i], expected[i]);
		igt_assert(expected[i] == data->bo_cpu_map[i]);
	}
}

/**
 * SUBTEST: render-copy-compressed-uncompressed-render-copy
 * Description: Basic compression test
 * Functionality: Test basic compression read and write
 * Test category: functionality test
 */

static void
render_copy_compress_uncompressed_render_copy(struct data *data)
{
	uint16_t dev_id = intel_get_drm_devid(data->fd);
	igt_render_copyfunc_t rendercopy = igt_get_render_copyfunc(dev_id);
	struct intel_bb *ibb;
	uint32_t *expected = malloc(NUM_DWORDS * sizeof(uint32_t));

	prepare_with_buf(data);
	draw_rectangle_to_cpu_bo(data, expected);

	/* copy cpu buffer to compressed using GPU */
	ibb = intel_bb_create_with_context(data->fd, data->render_exec_queue,
					   data->vm_id, NULL, 0x1000);
	rendercopy(ibb, &data->buf_cpu, 0, 0, WIDTH, HEIGHT,
		   &data->buf_compressed, 0, 0);
	intel_bb_destroy(ibb);

	/* set CPU buffer to 0 */
	memset(data->bo_cpu_map, 0, NUM_DWORDS * sizeof(uint32_t));

	/* copy compressed buffer to uncompressed buffer using GPU */
	ibb = intel_bb_create_with_context(data->fd, data->render_exec_queue,
					   data->vm_id, NULL, 0x1000);
	rendercopy(ibb, &data->buf_compressed, 0, 0, WIDTH, HEIGHT,
		   &data->buf_uncompressed, 0, 0);
	intel_bb_destroy(ibb);

	/* copy uncompressed buffer to CPU buffer using GPU */
	ibb = intel_bb_create_with_context(data->fd, data->render_exec_queue,
					   data->vm_id, NULL, 0x1000);
	rendercopy(ibb, &data->buf_uncompressed, 0, 0, WIDTH, HEIGHT,
		   &data->buf_cpu, 0, 0);
	intel_bb_destroy(ibb);

	verify(data, expected);
	free(expected);
	finish(data);
}

static void
blt(struct data *data, uint64_t src, uint64_t dst)
{
	struct drm_xe_sync sync = {
		.type = DRM_XE_SYNC_TYPE_SYNCOBJ,
		.flags = DRM_XE_SYNC_FLAG_SIGNAL,
	};
	struct drm_xe_exec exec = {
		.num_batch_buffer = 1,
		.num_syncs = 1,
		.syncs = to_user_pointer(&sync),
		.exec_queue_id = data->copy_exec_queue,
		.address = ADDR_BO_BATCH,
	};
	uint8_t mocs_index = intel_get_uc_mocs_index(data->fd);
	uint32_t *batch_map, batch_index = 0, syncobj;

	syncobj = syncobj_create(data->fd, 0);
	sync.handle = syncobj;

	batch_map = xe_bo_mmap_ext(data->fd, data->bo_batch_buffer,
				   data->batch_bufer_size, PROT_READ | PROT_WRITE);

	/* copy cpu buffer to compressed using GPU */
	/* MEM_COPY */
	batch_map[batch_index++] = (0x2 << 29) | (0x5a << 22) | (0x0 << 19) | (0x1 << 17) | 0x8;
	batch_map[batch_index++] = (WIDTH * sizeof(uint32_t)) + 1;
	batch_map[batch_index++] = HEIGHT;
	batch_map[batch_index++] = (WIDTH * sizeof(uint32_t));
	batch_map[batch_index++] = (WIDTH * sizeof(uint32_t));
	batch_map[batch_index++] = addr_low(src);
	batch_map[batch_index++] = addr_high(data->fd, src);
	batch_map[batch_index++] = addr_low(dst);
	batch_map[batch_index++] = addr_high(data->fd, dst);
	batch_map[batch_index++] = (mocs_index << 28) | (mocs_index << 3);

	batch_map[batch_index++] = MI_BATCH_BUFFER_END;
	munmap(batch_map, data->bo_batch_buffer);

	xe_exec(data->fd, &exec);
	igt_assert(syncobj_wait(data->fd, &syncobj, 1, INT64_MAX, 0, NULL));

	syncobj_destroy(data->fd, syncobj);
}

/**
 * SUBTEST: blt-basic
 * Description: Copy rectangle from CPU mapped buffer to compressed, compressed to CPU and verify.
 * Functionality: Test basic compression read and write
 * Test category: functionality test
 */

static void
blt_basic(struct data *data)
{
	uint32_t *expected = malloc(NUM_DWORDS * sizeof(uint32_t));

	prepare_with_buf(data);
	draw_rectangle_to_cpu_bo(data, expected);
	blt(data, ADDR_BO_CPU, ADDR_BO_COMPRESSED);
	memset(data->bo_cpu_map, 0, NUM_DWORDS * sizeof(uint32_t));
	blt(data, ADDR_BO_COMPRESSED, ADDR_BO_CPU);

	verify(data, expected);
	free(expected);
	finish(data);
}


/**
 * SUBTEST: blt-compressed-to-uncompressed
 * Description: Copy rectangle from CPU mapped buffer to compressed, compressed to uncompressed, uncompressed to CPU and verify.
 * Functionality: Test basic compression read and write
 * Test category: functionality test
 */

static void
blt_compressed_uncompressed(struct data *data)
{
	uint32_t *expected = malloc(NUM_DWORDS * sizeof(uint32_t));

	prepare_with_buf(data);
	draw_rectangle_to_cpu_bo(data, expected);
	blt(data, ADDR_BO_CPU, ADDR_BO_COMPRESSED);
	memset(data->bo_cpu_map, 0, NUM_DWORDS * sizeof(uint32_t));
	blt(data, ADDR_BO_COMPRESSED, ADDR_BO_UNCOMPRESSED);
	blt(data, ADDR_BO_UNCOMPRESSED, ADDR_BO_CPU);

	verify(data, expected);
	free(expected);
	finish(data);
}

/**
 * SUBTEST: blt-uncompressed-to-compressed
 * Description: Copy rectangle from CPU mapped buffer to uncompressed, uncompressed to compressed, compressed to CPU and verify.
 * Functionality: Test basic compression read and write
 * Test category: functionality test
 */

static void
blt_uncompressed_compressed(struct data *data)
{
	uint32_t *expected = malloc(NUM_DWORDS * sizeof(uint32_t));

	prepare_with_buf(data);
	draw_rectangle_to_cpu_bo(data, expected);
	blt(data, ADDR_BO_CPU, ADDR_BO_UNCOMPRESSED);
	memset(data->bo_cpu_map, 0, NUM_DWORDS * sizeof(uint32_t));
	blt(data, ADDR_BO_UNCOMPRESSED, ADDR_BO_COMPRESSED);
	blt(data, ADDR_BO_COMPRESSED, ADDR_BO_CPU);

	verify(data, expected);
	free(expected);
	finish(data);
}

/**
 * SUBTEST: blt-compressed-to-zero-uncompressed
 * Description: Copy rectangle from CPU mapped buffer to uncompressed, uncompressed to compressed, compressed to CPU and verify.
 * Functionality: Test basic compression read and write
 * Test category: functionality test
 */

static void
blt_compressed_zero_uncompressed(struct data *data)
{
	uint32_t *expected = malloc(NUM_DWORDS * sizeof(uint32_t));

	prepare_with_buf(data);
	draw_rectangle_to_cpu_bo(data, expected);

	/* copy rectangle to compressed and verify */
	blt(data, ADDR_BO_CPU, ADDR_BO_COMPRESSED);
	memset(data->bo_cpu_map, 0, NUM_DWORDS * sizeof(uint32_t));
	blt(data, ADDR_BO_COMPRESSED, ADDR_BO_CPU);
	verify(data, expected);

	/* set CPU buffer to 0, update expected buffer, copy from cpu to compressed */
	memset(data->bo_cpu_map, 0, NUM_DWORDS * sizeof(uint32_t));
	memcpy(expected, data->bo_cpu_map, NUM_DWORDS * sizeof(uint32_t));
	blt(data, ADDR_BO_CPU, ADDR_BO_COMPRESSED);

	blt(data, ADDR_BO_COMPRESSED, ADDR_BO_UNCOMPRESSED);
	blt(data, ADDR_BO_UNCOMPRESSED, ADDR_BO_CPU);

	verify(data, expected);
	free(expected);
	finish(data);
}

/**
 * SUBTEST: blt-compressed-to-zero-uncompressed2
 * Description: Copy rectangle from CPU mapped buffer to uncompressed, uncompressed to compressed, compressed to CPU and verify.
 * Functionality: Test basic compression read and write
 * Test category: functionality test
 */

static void
blt_compressed_zero_uncompressed2(struct data *data)
{
	uint32_t *expected = malloc(NUM_DWORDS * sizeof(uint32_t));

	prepare_with_buf(data);
	draw_rectangle_to_cpu_bo(data, expected);

	/* copy rectangle to compressed and verify */
	blt(data, ADDR_BO_CPU, ADDR_BO_COMPRESSED);
	memset(data->bo_cpu_map, 0, NUM_DWORDS * sizeof(uint32_t));
	blt(data, ADDR_BO_COMPRESSED, ADDR_BO_CPU);
	verify(data, expected);

	/* set CPU buffer to 0, update expected buffer, copy from cpu to uncompressed */
	memset(data->bo_cpu_map, 0, NUM_DWORDS * sizeof(uint32_t));
	memcpy(expected, data->bo_cpu_map, NUM_DWORDS * sizeof(uint32_t));
	blt(data, ADDR_BO_CPU, ADDR_BO_UNCOMPRESSED);

	blt(data, ADDR_BO_UNCOMPRESSED, ADDR_BO_CPU);

	verify(data, expected);
	free(expected);
	finish(data);
}

/**
 * SUBTEST: blt-compressed-to-zero-uncompressed3
 * Description: Copy rectangle from CPU mapped buffer to uncompressed, uncompressed to compressed, compressed to CPU and verify.
 * Functionality: Test basic compression read and write
 * Test category: functionality test
 */

static void
blt_compressed_zero_uncompressed3(struct data *data)
{
	uint32_t *expected = malloc(NUM_DWORDS * sizeof(uint32_t));

	prepare_with_buf(data);
	draw_rectangle_to_cpu_bo(data, expected);

	/* copy rectangle to uncompressed and verify */
	blt(data, ADDR_BO_CPU, ADDR_BO_UNCOMPRESSED);
	memset(data->bo_cpu_map, 0, NUM_DWORDS * sizeof(uint32_t));
	blt(data, ADDR_BO_UNCOMPRESSED, ADDR_BO_CPU);
	verify(data, expected);

	/* set CPU buffer to 0, update expected buffer, copy from cpu to compressed */
	memset(data->bo_cpu_map, 0, NUM_DWORDS * sizeof(uint32_t));
	memcpy(expected, data->bo_cpu_map, NUM_DWORDS * sizeof(uint32_t));
	blt(data, ADDR_BO_CPU, ADDR_BO_COMPRESSED);

	blt(data, ADDR_BO_COMPRESSED, ADDR_BO_CPU);

	verify(data, expected);
	free(expected);
	finish(data);
}

/**
 * SUBTEST: blt-compressed-to-zero-uncompressed4
 * Description: Copy rectangle from CPU mapped buffer to uncompressed, uncompressed to compressed, compressed to CPU and verify.
 * Functionality: Test basic compression read and write
 * Test category: functionality test
 */

static void
blt_compressed_zero_uncompressed4(struct data *data)
{
	uint32_t *expected = malloc(NUM_DWORDS * sizeof(uint32_t));

	prepare_with_buf(data);
	write_coffee_cpu_map(data);
	mi_copy_dwords(data, ADDR_BO_CPU, ADDR_BO_COMPRESSED, NUM_DWORDS);
	mi_copy_dwords(data, ADDR_BO_COMPRESSED, ADDR_BO_CPU, NUM_DWORDS);
	memset(data->bo_cpu_map, 0, NUM_DWORDS * sizeof(uint32_t));
	check_coffee_cpu_map(data);

	draw_rectangle_to_cpu_bo(data, expected);
	blt(data, ADDR_BO_CPU, ADDR_BO_UNCOMPRESSED);
	memset(data->bo_cpu_map, 0, NUM_DWORDS * sizeof(uint32_t));
	blt(data, ADDR_BO_UNCOMPRESSED, ADDR_BO_CPU);
	verify(data, expected);

	blt(data, ADDR_BO_CPU, ADDR_BO_COMPRESSED);
	memset(data->bo_cpu_map, 0, NUM_DWORDS * sizeof(uint32_t));
	blt(data, ADDR_BO_COMPRESSED, ADDR_BO_CPU);
	verify(data, expected);

	write_coffee_cpu_map(data);
	mi_copy_dwords(data, ADDR_BO_CPU, ADDR_BO_UNCOMPRESSED, NUM_DWORDS);
	mi_copy_dwords(data, ADDR_BO_UNCOMPRESSED, ADDR_BO_CPU, NUM_DWORDS);
	memset(data->bo_cpu_map, 0, NUM_DWORDS * sizeof(uint32_t));
	check_coffee_cpu_map(data);

	free(expected);
	finish(data);
}

/**
 * SUBTEST: render-copy-basic
 * Description: Basic compression test
 * Functionality: Test basic compression read and write
 * Test category: functionality test
 */

static void
render_copy_basic(struct data *data)
{
	uint16_t dev_id = intel_get_drm_devid(data->fd);
	igt_render_copyfunc_t rendercopy = igt_get_render_copyfunc(dev_id);
	struct intel_bb *ibb;
	uint32_t *expected = malloc(NUM_DWORDS * sizeof(uint32_t));

	prepare_with_buf(data);

	draw_rectangle_to_cpu_bo(data, expected);

	/* copy cpu buffer to compressed using GPU */
	ibb = intel_bb_create_with_context(data->fd, data->render_exec_queue,
					   data->vm_id, NULL, 0x1000);
	rendercopy(ibb, &data->buf_cpu, 0, 0, WIDTH, HEIGHT,
		   &data->buf_compressed, 0, 0);
	intel_bb_destroy(ibb);

	/* set CPU buffer to 0 */
	memset(data->bo_cpu_map, 0, NUM_DWORDS * sizeof(uint32_t));

	/* copy compressed buffer to CPU buffer using GPU */
	ibb = intel_bb_create_with_context(data->fd, data->render_exec_queue,
					   data->vm_id, NULL, 0x1000);
	rendercopy(ibb, &data->buf_compressed, 0, 0, WIDTH, HEIGHT,
		   &data->buf_cpu, 0, 0);
	intel_bb_destroy(ibb);

	verify(data, expected);
	free(expected);
	finish(data);
}

/**
 * SUBTEST: mi-basic
 * Description: Basic compression test
 * Functionality: Test basic compression read and write
 * Test category: functionality test
 */

static void
mi_basic(struct data *data)
{
	prepare(data);

	write_coffee_cpu_map(data);
	mi_copy_dwords(data, ADDR_BO_CPU, ADDR_BO_COMPRESSED, NUM_DWORDS);
	memset(data->bo_cpu_map, 0, NUM_DWORDS * sizeof(uint32_t));
	munmap(data->bo_cpu_map, data->buffer_size);
	data->bo_cpu_map = NULL;

	mi_copy_dwords(data, ADDR_BO_COMPRESSED, ADDR_BO_CPU, NUM_DWORDS);

	data->bo_cpu_map = xe_bo_mmap_ext(data->fd, data->bo_cpu, data->buffer_size,
					  PROT_READ | PROT_WRITE);
	check_coffee_cpu_map(data);

	finish(data);
}

/**
 * SUBTEST: mi-compressed-to-uncompressed
 * Description: Test resolve pass from compressed to uncompressed buffer
 * Functionality: compression
 * Test category: functionality test
 */

static void
mi_compressed_to_uncompressed(struct data *data)
{
	prepare(data);

	write_coffee_cpu_map(data);
	mi_copy_dwords(data, ADDR_BO_CPU, ADDR_BO_COMPRESSED, NUM_DWORDS);
	memset(data->bo_cpu_map, 0, NUM_DWORDS * sizeof(uint32_t));
	munmap(data->bo_cpu_map, data->buffer_size);
	data->bo_cpu_map = NULL;

	mi_copy_dwords_with_flush(data, ADDR_BO_COMPRESSED, ADDR_BO_UNCOMPRESSED, NUM_DWORDS, true);
	mi_copy_dwords_with_flush(data, ADDR_BO_UNCOMPRESSED, ADDR_BO_CPU, NUM_DWORDS, true);

	data->bo_cpu_map = xe_bo_mmap_ext(data->fd, data->bo_cpu, data->buffer_size,
					  PROT_READ | PROT_WRITE);
	check_coffee_cpu_map(data);
	finish(data);
}

/**
 * SUBTEST: mi-uncompressed-to-compressed
 * Description: Test resolve pass from uncompressed to compressed buffer
 * Functionality: compression
 * Test category: functionality test
 */

static void
mi_uncompressed_to_compressed(struct data *data)
{
	prepare(data);

	write_coffee_cpu_map(data);
	mi_copy_dwords(data, ADDR_BO_CPU, ADDR_BO_COMPRESSED, NUM_DWORDS);
	memset(data->bo_cpu_map, 0, NUM_DWORDS * sizeof(uint32_t));
	munmap(data->bo_cpu_map, data->buffer_size);
	data->bo_cpu_map = NULL;

	mi_copy_dwords_with_flush(data, ADDR_BO_UNCOMPRESSED, ADDR_BO_COMPRESSED, NUM_DWORDS, true);
	mi_copy_dwords_with_flush(data, ADDR_BO_COMPRESSED, ADDR_BO_CPU, NUM_DWORDS, true);
	check_coffee_cpu_map(data);

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
		data.render_exec_queue = xe_exec_queue_create_class(data.fd, data.vm_id, DRM_XE_ENGINE_CLASS_RENDER);
		data.copy_exec_queue = xe_exec_queue_create_class(data.fd, data.vm_id, DRM_XE_ENGINE_CLASS_COPY);
		data.bops = buf_ops_create(data.fd);
	}

	igt_subtest("mi-basic")
		mi_basic(&data);

	/* Not working probably because MI can't do compressino resolve */
	igt_subtest("mi-compressed-to-uncompressed")
		mi_compressed_to_uncompressed(&data);

	/* Not working probably because MI can't do compressino resolve */
	igt_subtest("mi-uncompressed-to-compressed")
		mi_uncompressed_to_compressed(&data);

	igt_subtest("render-copy-basic")
		render_copy_basic(&data);

	/* Not working because intel_bb can't have the same bo with 2 different addesses */
	igt_subtest("render-copy-compressed-uncompressed-render-copy")
		render_copy_compress_uncompressed_render_copy(&data);

	igt_subtest("blt-basic")
		blt_basic(&data);

	igt_subtest("blt-compressed-to-uncompressed")
		blt_compressed_uncompressed(&data);

	igt_subtest("blt-uncompressed-to-compressed")
		blt_uncompressed_compressed(&data);

	igt_subtest("blt-compressed-to-zero-uncompressed")
		blt_compressed_zero_uncompressed(&data);

	igt_subtest("blt-compressed-to-zero-uncompressed2")
		blt_compressed_zero_uncompressed2(&data);

	igt_subtest("blt-compressed-to-zero-uncompressed3")
		blt_compressed_zero_uncompressed3(&data);

	/* not working, did not debugged */
	igt_subtest("blt-compressed-to-zero-uncompressed4")
		blt_compressed_zero_uncompressed4(&data);

	igt_fixture {
		buf_ops_destroy(data.bops);
		xe_exec_queue_destroy(data.fd, data.render_exec_queue);
		xe_exec_queue_destroy(data.fd, data.copy_exec_queue);
		xe_vm_destroy(data.fd, data.vm_id);
		drm_close_driver(data.fd);
	}
}