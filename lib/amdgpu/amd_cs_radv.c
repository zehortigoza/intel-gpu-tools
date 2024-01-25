// SPDX-License-Identifier: MIT
/*
 * Copyright 2024 Advanced Micro Devices, Inc.
 */
#include <time.h>
#include <sys/time.h>
#include <amdgpu_drm.h>

#include "lib/amdgpu/amd_memory.h"
#include "lib/amdgpu/amd_sdma.h"
#include "lib/amdgpu/amd_PM4.h"
#include "lib/amdgpu/amd_cs_radv.h"

#define TIME_MONOTONIC 2
#define OS_TIMEOUT_INFINITE 0xffffffffffffffffull

static bool
amdgpu_cs_has_user_fence(struct amdgpu_cs_request_radv *request)
{
	return request->ip_type != AMDGPU_HW_IP_UVD && request->ip_type != AMDGPU_HW_IP_VCE &&
			request->ip_type != AMDGPU_HW_IP_UVD_ENC && request->ip_type != AMDGPU_HW_IP_VCN_DEC &&
			request->ip_type != AMDGPU_HW_IP_VCN_ENC && request->ip_type != AMDGPU_HW_IP_VCN_JPEG;
}

static int64_t
os_time_get_nano(void)
{
	struct timespec ts;

	timespec_get(&ts, TIME_MONOTONIC);
	return ts.tv_nsec + ts.tv_sec*INT64_C(1000000000);
}

static int64_t
os_time_get_absolute_timeout(uint64_t timeout)
{
	int64_t time, abs_timeout;

	/* Also check for the type upper bound. */
	if (timeout == OS_TIMEOUT_INFINITE || timeout > INT64_MAX)
		return OS_TIMEOUT_INFINITE;

	time = os_time_get_nano();
	abs_timeout = time + (int64_t)timeout;

	/* Check for overflow. */
	if (abs_timeout < time)
		return OS_TIMEOUT_INFINITE;

	return abs_timeout;
}

static void
os_time_sleep(int64_t usecs)
{
	struct timespec time;

	time.tv_sec = usecs / 1000000;
	time.tv_nsec = (usecs % 1000000) * 1000;
	while (clock_nanosleep(CLOCK_MONOTONIC, 0, &time, &time) == EINTR)
		;
}

uint32_t
amdgpu_get_bo_handle(struct amdgpu_bo *bo)
{
	uint32_t handle;
	int r;

	r = amdgpu_bo_export(bo, amdgpu_bo_handle_type_kms, &handle);
	igt_assert_eq(r, 0);
	return handle;
}

static uint32_t
radv_to_amdgpu_priority(enum amdgpu_ctx_priority_radv radv_priority)
{
	switch (radv_priority) {
	case AMDGPU_IGT_CTX_PRIORITY_REALTIME:
		return AMDGPU_CTX_PRIORITY_VERY_HIGH;
	case AMDGPU_IGT_CTX_PRIORITY_HIGH:
		return AMDGPU_CTX_PRIORITY_HIGH;
	case AMDGPU_IGT_CTX_PRIORITY_MEDIUM:
		return AMDGPU_CTX_PRIORITY_NORMAL;
	case AMDGPU_IGT_CTX_PRIORITY_LOW:
		return AMDGPU_CTX_PRIORITY_LOW;
	default:
		return AMDGPU_CTX_PRIORITY_NORMAL;
	}
}

uint32_t
amdgpu_ctx_radv_create(amdgpu_device_handle device,
		enum amdgpu_ctx_priority_radv priority, struct amdgpu_ctx_radv **rctx)
{
	struct amdgpu_ctx_radv *ctx;
	uint32_t amdgpu_priority, r;

	ctx = calloc(1, sizeof(*ctx));
	igt_assert(ctx);
	ctx->fence_bo = calloc(1, sizeof(*ctx->fence_bo));
	igt_assert(ctx->fence_bo);

	amdgpu_priority = radv_to_amdgpu_priority(priority);
	r = amdgpu_cs_ctx_create2(device, amdgpu_priority, &ctx->ctx);

	assert(AMDGPU_HW_IP_NUM * MAX_RINGS_PER_TYPE * 4 * sizeof(uint64_t) <= 4096);
	ctx->fence_bo->size = 4096;

	ctx->fence_bo->bo = gpu_mem_alloc(device, ctx->fence_bo->size, 8, AMDGPU_GEM_DOMAIN_GTT,
			AMDGPU_GEM_CREATE_CPU_ACCESS_REQUIRED, &ctx->fence_bo->vmc_addr,
			&ctx->fence_bo->va_handle);

	*rctx = ctx;
	return r;

}

void
amdgpu_ctx_radv_destroy(amdgpu_device_handle device, struct amdgpu_ctx_radv *rwctx)
{
	unsigned int ip, ring;

	for (ip = 0; ip <= AMDGPU_HW_IP_NUM; ++ip) {
		for (ring = 0; ring < MAX_RINGS_PER_TYPE; ++ring) {
			if (rwctx->queue_syncobj[ip][ring])
				amdgpu_cs_destroy_syncobj(device, rwctx->queue_syncobj[ip][ring]);
		}
	}
	gpu_mem_free(rwctx->fence_bo->bo,
				rwctx->fence_bo->va_handle,
				rwctx->fence_bo->vmc_addr,
				rwctx->fence_bo->size);
	free(rwctx->fence_bo);
	amdgpu_cs_ctx_free(rwctx->ctx);
	free(rwctx);
}

uint32_t
amdgpu_cs_submit_radv(amdgpu_device_handle dev, struct amdgpu_ring_context *ring_context,
		struct amdgpu_cs_request_radv *request, struct amdgpu_ctx_radv *ctx)
{
	int r, num_chunks,  size, i;
	struct drm_amdgpu_cs_chunk *chunks;
	struct drm_amdgpu_cs_chunk_data *chunk_data;
	struct drm_amdgpu_bo_list_in bo_list_in;
	struct amdgpu_cs_fence_info fence_info;
	uint32_t result = 0;
	uint64_t abs_timeout_ns;
	bool has_user_fence;

	has_user_fence = amdgpu_cs_has_user_fence(request);
	size = request->number_of_ibs + 1 + (has_user_fence ? 1 : 0) + 1 /* bo list */ + 3;
	chunks = malloc(sizeof(chunks[0]) * size);
	size = request->number_of_ibs + (has_user_fence ? 1 : 0);
	chunk_data = malloc(sizeof(chunk_data[0]) * size);

	num_chunks = request->number_of_ibs;
	for (i = 0; i < request->number_of_ibs; i++) {

		struct amdgpu_cs_ib_info_radv *ib;

		chunks[i].chunk_id = AMDGPU_CHUNK_ID_IB;
		chunks[i].length_dw = sizeof(struct drm_amdgpu_cs_chunk_ib) / 4;
		chunks[i].chunk_data = (uint64_t)(uintptr_t)&chunk_data[i];

		ib = &request->ibs[i];
		assert(ib->size);

		chunk_data[i].ib_data._pad = 0;
		chunk_data[i].ib_data.va_start = ib->ib_mc_address;
		chunk_data[i].ib_data.ib_bytes = ib->size * 4;
		chunk_data[i].ib_data.ip_type = ib->ip_type;
		chunk_data[i].ib_data.flags = ib->flags;

		chunk_data[i].ib_data.ip_instance = request->ip_instance;
		chunk_data[i].ib_data.ring = request->ring;
	}

	assert(chunk_data[request->number_of_ibs - 1].ib_data.ip_type == request->ip_type);

	if (has_user_fence) {
		i = num_chunks++;
		chunks[i].chunk_id = AMDGPU_CHUNK_ID_FENCE;
		chunks[i].length_dw = sizeof(struct drm_amdgpu_cs_chunk_fence) / 4;
		chunks[i].chunk_data = (uint64_t)(uintptr_t)&chunk_data[i];

		fence_info.handle = ctx->fence_bo->bo;
		/* Need to reserve 4 QWORD for user fence:
		 * QWORD[0]: completed fence
		 * QWORD[1]: preempted fence
		 * QWORD[2]: reset fence
		 * QWORD[3]: preempted then reset
		 */
		fence_info.offset = (request->ip_type * MAX_RINGS_PER_TYPE + request->ring) * 4;
		amdgpu_cs_chunk_fence_info_to_data(&fence_info, &chunk_data[i]);
	}

	bo_list_in.operation = ~0;
	bo_list_in.list_handle = ~0;
	bo_list_in.bo_number = request->num_handles;
	bo_list_in.bo_info_size = sizeof(struct drm_amdgpu_bo_list_entry);
	bo_list_in.bo_info_ptr = (uint64_t)(uintptr_t)request->handles;

	chunks[num_chunks].chunk_id = AMDGPU_CHUNK_ID_BO_HANDLES;
	chunks[num_chunks].length_dw = sizeof(struct drm_amdgpu_bo_list_in) / 4;
	chunks[num_chunks].chunk_data = (uintptr_t)&bo_list_in;
	num_chunks++;

	/* The kernel returns -ENOMEM with many parallel processes using GDS such as test suites quite
	 * often, but it eventually succeeds after enough attempts. This happens frequently with dEQP
	 * using NGG streamout.
	 */
	 abs_timeout_ns = os_time_get_absolute_timeout(1000000000ull); /* 1s */

	r = 0;
	do {
		/* Wait 1 ms and try again. */
		if (r == -ENOMEM)
			os_time_sleep(1000);

		r = amdgpu_cs_submit_raw2(dev, ctx->ctx, 0,
				num_chunks, chunks, &request->seq_no);
	} while (r == -ENOMEM && os_time_get_nano() < abs_timeout_ns);

	if (r) {
		if (r == -ENOMEM) {
			igt_info("igt/amdgpu: Not enough memory for command submission.\n");
			result = ENOMEM;
		} else if (r == -ECANCELED) {
			igt_info("igt/amdgpu: The CS has been cancelled because the context is lost.\n");
			result = ECANCELED;
		} else {
			igt_info("igt/amdgpu: The CS has been rejected, see dmesg for more information (%i).\n", r);
			result = EINVAL;
		}
	}
	free(chunks);
	free(chunk_data);
	return result;
}
