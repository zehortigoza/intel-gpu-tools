/* SPDX-License-Identifier: MIT
 * Copyright 2024 Advanced Micro Devices, Inc.
 */

#ifndef AMD_CS_RADV
#define AMD_CS_RADV

#include "amd_ip_blocks.h"
#define AMDGPU_CS_GANG_SIZE	4

enum amdgpu_ctx_priority_radv {
	AMDGPU_IGT_CTX_PRIORITY_LOW = 0,
	AMDGPU_IGT_CTX_PRIORITY_MEDIUM,
	AMDGPU_IGT_CTX_PRIORITY_HIGH,
	AMDGPU_IGT_CTX_PRIORITY_REALTIME,
};

struct amdgpu_cs_ib_info_radv {
	int64_t flags;
	uint64_t ib_mc_address;
	uint32_t size;
	enum amd_ip_block_type ip_type;
};

enum { MAX_RINGS_PER_TYPE = 8 };

struct amdgpu_fence_radv {
	struct amdgpu_cs_fence fence;
};

struct amdgpu_winsys_bo_radv {
	amdgpu_va_handle va_handle;
	uint64_t vmc_addr;
	uint64_t size;
	bool is_virtual;
	uint8_t priority;

	union {
		/* physical bo */
		struct {
			amdgpu_bo_handle bo;
			uint32_t bo_handle;
		};
		/* virtual bo */
		struct {
			uint32_t range_count;
			uint32_t range_capacity;
			struct amdgpu_winsys_bo_radv **bos;
			uint32_t bo_count;
			uint32_t bo_capacity;
		};
	};
};


struct amdgpu_ctx_radv {
	amdgpu_context_handle ctx;
	struct amdgpu_fence_radv last_submission[AMDGPU_HW_IP_NUM + 1][MAX_RINGS_PER_TYPE];
	struct amdgpu_winsys_bo_radv *fence_bo;

	uint32_t queue_syncobj[AMDGPU_HW_IP_NUM + 1][MAX_RINGS_PER_TYPE];
	bool queue_syncobj_wait[AMDGPU_HW_IP_NUM + 1][MAX_RINGS_PER_TYPE];
};



struct amdgpu_cs_request_radv {
	/** Specify HW IP block type to which to send the IB. */
	uint32_t ip_type;

	/** IP instance index if there are several IPs of the same type. */
	uint32_t ip_instance;

	/**
	 * Specify ring index of the IP. We could have several rings
	 * in the same IP. E.g. 0 for SDMA0 and 1 for SDMA1.
	 */
	uint32_t ring;

	/**
	 * BO list handles used by this request.
	 */
	struct drm_amdgpu_bo_list_entry *handles;
	uint32_t num_handles;

	/** Number of IBs to submit in the field ibs. */
	uint32_t number_of_ibs;

	/**
	 * IBs to submit. Those IBs will be submitted together as single entity
	 */
	struct amdgpu_cs_ib_info_radv ibs[AMDGPU_CS_GANG_SIZE];
	/**
	 * The returned sequence number for the command submission
	 */
	uint64_t seq_no;
};

uint32_t
amdgpu_get_bo_handle(struct amdgpu_bo *bo);

uint32_t
amdgpu_ctx_radv_create(amdgpu_device_handle device,
		enum amdgpu_ctx_priority_radv priority, struct amdgpu_ctx_radv **rctx);
void
amdgpu_ctx_radv_destroy(amdgpu_device_handle device, struct amdgpu_ctx_radv *rwctx);

uint32_t
amdgpu_cs_submit_radv(amdgpu_device_handle device, struct amdgpu_ring_context *ring_context,
		struct amdgpu_cs_request_radv *request,  struct amdgpu_ctx_radv *ctx);

#endif
