// SPDX-License-Identifier: MIT
/*
 * Copyright © 2023 Intel Corporation
 *
 * Authors:
 *    Jason Ekstrand <jason@jlekstrand.net>
 *    Maarten Lankhorst <maarten.lankhorst@linux.intel.com>
 *    Matthew Brost <matthew.brost@intel.com>
 */

#ifdef HAVE_LIBGEN_H
#include <libgen.h>
#endif

#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <pciaccess.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>

#ifdef __linux__
#include <sys/sysmacros.h>
#else
#define major(__v__) (((__v__) >> 8) & 0xff)
#define minor(__v__) ((__v__) & 0xff)
#endif
#include <time.h>

#include "config.h"
#include "drmtest.h"
#include "igt_syncobj.h"
#include "intel_pat.h"
#include "ioctl_wrappers.h"
#include "xe_ioctl.h"
#include "xe_query.h"

uint32_t xe_cs_prefetch_size(int fd)
{
	return 4096;
}

uint64_t xe_bb_size(int fd, uint64_t reqsize)
{
	return ALIGN(reqsize + xe_cs_prefetch_size(fd),
	             xe_get_default_alignment(fd));
}

uint32_t xe_vm_create(int fd, uint32_t flags, uint64_t ext)
{
	struct drm_xe_vm_create create = {
		.extensions = ext,
		.flags = flags,
	};

	igt_assert_eq(igt_ioctl(fd, DRM_IOCTL_XE_VM_CREATE, &create), 0);

	return create.vm_id;
}

void xe_vm_unbind_all_async(int fd, uint32_t vm, uint32_t exec_queue,
			    uint32_t bo, struct drm_xe_sync *sync,
			    uint32_t num_syncs)
{
	__xe_vm_bind_assert(fd, vm, exec_queue, bo, 0, 0, 0,
			    DRM_XE_VM_BIND_OP_UNMAP_ALL, 0,
			    sync, num_syncs, 0, 0);
}

void xe_vm_bind_array(int fd, uint32_t vm, uint32_t exec_queue,
		      struct drm_xe_vm_bind_op *bind_ops,
		      uint32_t num_bind, struct drm_xe_sync *sync,
		      uint32_t num_syncs)
{
	struct drm_xe_vm_bind bind = {
		.vm_id = vm,
		.num_binds = num_bind,
		.vector_of_binds = (uintptr_t)bind_ops,
		.num_syncs = num_syncs,
		.syncs = (uintptr_t)sync,
		.exec_queue_id = exec_queue,
	};

	igt_assert(num_bind > 1);
	igt_assert_eq(igt_ioctl(fd, DRM_IOCTL_XE_VM_BIND, &bind), 0);
}

int  __xe_vm_bind(int fd, uint32_t vm, uint32_t exec_queue, uint32_t bo,
		  uint64_t offset, uint64_t addr, uint64_t size, uint32_t op,
		  uint32_t flags, struct drm_xe_sync *sync, uint32_t num_syncs,
		  uint32_t prefetch_region, uint8_t pat_index, uint64_t ext)
{
	struct drm_xe_vm_bind bind = {
		.extensions = ext,
		.vm_id = vm,
		.num_binds = 1,
		.bind.obj = bo,
		.bind.obj_offset = offset,
		.bind.range = size,
		.bind.addr = addr,
		.bind.op = op,
		.bind.flags = flags,
		.bind.prefetch_mem_region_instance = prefetch_region,
		.num_syncs = num_syncs,
		.syncs = (uintptr_t)sync,
		.exec_queue_id = exec_queue,
		.bind.pat_index = (pat_index == DEFAULT_PAT_INDEX) ?
			intel_get_pat_idx_wb(fd) : pat_index,
	};

	if (igt_ioctl(fd, DRM_IOCTL_XE_VM_BIND, &bind))
		return -errno;

	return 0;
}

void  __xe_vm_bind_assert(int fd, uint32_t vm, uint32_t exec_queue, uint32_t bo,
			  uint64_t offset, uint64_t addr, uint64_t size,
			  uint32_t op, uint32_t flags, struct drm_xe_sync *sync,
			  uint32_t num_syncs, uint32_t prefetch_region, uint64_t ext)
{
	igt_assert_eq(__xe_vm_bind(fd, vm, exec_queue, bo, offset, addr, size,
				   op, flags, sync, num_syncs, prefetch_region,
				   DEFAULT_PAT_INDEX, ext), 0);
}

void xe_vm_prefetch_async(int fd, uint32_t vm, uint32_t exec_queue, uint64_t offset,
			  uint64_t addr, uint64_t size,
			  struct drm_xe_sync *sync, uint32_t num_syncs,
			  uint32_t region)
{
	__xe_vm_bind_assert(fd, vm, exec_queue, 0, offset, addr, size,
			    DRM_XE_VM_BIND_OP_PREFETCH, 0,
			    sync, num_syncs, region, 0);
}

void xe_vm_bind_async(int fd, uint32_t vm, uint32_t exec_queue, uint32_t bo,
		      uint64_t offset, uint64_t addr, uint64_t size,
		      struct drm_xe_sync *sync, uint32_t num_syncs)
{
	__xe_vm_bind_assert(fd, vm, exec_queue, bo, offset, addr, size,
			    DRM_XE_VM_BIND_OP_MAP, 0, sync,
			    num_syncs, 0, 0);
}

void xe_vm_bind_async_flags(int fd, uint32_t vm, uint32_t exec_queue, uint32_t bo,
			    uint64_t offset, uint64_t addr, uint64_t size,
			    struct drm_xe_sync *sync, uint32_t num_syncs,
			    uint32_t flags)
{
	__xe_vm_bind_assert(fd, vm, exec_queue, bo, offset, addr, size,
			    DRM_XE_VM_BIND_OP_MAP, flags,
			    sync, num_syncs, 0, 0);
}

void xe_vm_bind_userptr_async(int fd, uint32_t vm, uint32_t exec_queue,
			      uint64_t userptr, uint64_t addr, uint64_t size,
			      struct drm_xe_sync *sync, uint32_t num_syncs)
{
	__xe_vm_bind_assert(fd, vm, exec_queue, 0, userptr, addr, size,
			    DRM_XE_VM_BIND_OP_MAP_USERPTR, 0,
			    sync, num_syncs, 0, 0);
}

void xe_vm_bind_userptr_async_flags(int fd, uint32_t vm, uint32_t exec_queue,
				    uint64_t userptr, uint64_t addr,
				    uint64_t size, struct drm_xe_sync *sync,
				    uint32_t num_syncs, uint32_t flags)
{
	__xe_vm_bind_assert(fd, vm, exec_queue, 0, userptr, addr, size,
			    DRM_XE_VM_BIND_OP_MAP_USERPTR, flags,
			    sync, num_syncs, 0, 0);
}

void xe_vm_unbind_async(int fd, uint32_t vm, uint32_t exec_queue,
			uint64_t offset, uint64_t addr, uint64_t size,
			struct drm_xe_sync *sync, uint32_t num_syncs)
{
	__xe_vm_bind_assert(fd, vm, exec_queue, 0, offset, addr, size,
			    DRM_XE_VM_BIND_OP_UNMAP, 0, sync,
			    num_syncs, 0, 0);
}

static void __xe_vm_bind_sync(int fd, uint32_t vm, uint32_t bo, uint64_t offset,
			      uint64_t addr, uint64_t size, uint32_t op)
{
	struct drm_xe_sync sync = {
		.type = DRM_XE_SYNC_TYPE_SYNCOBJ,
		.flags = DRM_XE_SYNC_FLAG_SIGNAL,
		.handle = syncobj_create(fd, 0),
	};

	__xe_vm_bind_assert(fd, vm, 0, bo, offset, addr, size, op, 0, &sync, 1,
			    0, 0);

	igt_assert(syncobj_wait(fd, &sync.handle, 1, INT64_MAX, 0, NULL));
	syncobj_destroy(fd, sync.handle);
}

void xe_vm_bind_sync(int fd, uint32_t vm, uint32_t bo, uint64_t offset,
		     uint64_t addr, uint64_t size)
{
	__xe_vm_bind_sync(fd, vm, bo, offset, addr, size, DRM_XE_VM_BIND_OP_MAP);
}

void xe_vm_unbind_sync(int fd, uint32_t vm, uint64_t offset,
		       uint64_t addr, uint64_t size)
{
	__xe_vm_bind_sync(fd, vm, 0, offset, addr, size, DRM_XE_VM_BIND_OP_UNMAP);
}

void xe_vm_destroy(int fd, uint32_t vm)
{
	struct drm_xe_vm_destroy destroy = {
		.vm_id = vm,
	};

	igt_assert_eq(igt_ioctl(fd, DRM_IOCTL_XE_VM_DESTROY, &destroy), 0);
}

uint16_t __xe_default_cpu_caching(int fd, uint32_t placement, uint32_t flags)
{
	if ((placement & all_memory_regions(fd)) != system_memory(fd) ||
	    flags & DRM_XE_GEM_CREATE_FLAG_SCANOUT)
		/* VRAM placements or scanout should always use WC */
		return DRM_XE_GEM_CPU_CACHING_WC;

	return DRM_XE_GEM_CPU_CACHING_WB;
}

static bool vram_selected(int fd, uint32_t selected_regions)
{
	uint64_t regions = all_memory_regions(fd) & selected_regions;
	uint64_t region;

	xe_for_each_mem_region(fd, regions, region)
		if (xe_mem_region(fd, region)->mem_class == DRM_XE_MEM_REGION_CLASS_VRAM)
			return true;

	return false;
}

static uint32_t ___xe_bo_create(int fd, uint32_t vm, uint64_t size, uint32_t placement,
				uint32_t flags, uint16_t cpu_caching, uint32_t *handle)
{
	struct drm_xe_gem_create create = {
		.vm_id = vm,
		.size = size,
		.placement = placement,
		.flags = flags,
		.cpu_caching = cpu_caching,
	};
	int err;

	/*
	 * In case vram_if_possible returned system_memory,
	 * visible VRAM cannot be requested through flags
	 */
	if (!vram_selected(fd, placement))
		create.flags &= ~DRM_XE_GEM_CREATE_FLAG_NEEDS_VISIBLE_VRAM;

	err = igt_ioctl(fd, DRM_IOCTL_XE_GEM_CREATE, &create);
	if (err)
		return err;

	*handle = create.handle;
	return 0;

}

uint32_t __xe_bo_create(int fd, uint32_t vm, uint64_t size, uint32_t placement,
			uint32_t flags, uint32_t *handle)
{
	uint16_t cpu_caching = __xe_default_cpu_caching(fd, placement, flags);

	return ___xe_bo_create(fd, vm, size, placement, flags, cpu_caching, handle);
}

uint32_t xe_bo_create(int fd, uint32_t vm, uint64_t size, uint32_t placement,
		      uint32_t flags)
{
	uint32_t handle;

	igt_assert_eq(__xe_bo_create(fd, vm, size, placement, flags, &handle), 0);

	return handle;
}

uint32_t __xe_bo_create_caching(int fd, uint32_t vm, uint64_t size, uint32_t placement,
				uint32_t flags, uint16_t cpu_caching, uint32_t *handle)
{
	return ___xe_bo_create(fd, vm, size, placement, flags, cpu_caching, handle);
}

uint32_t xe_bo_create_caching(int fd, uint32_t vm, uint64_t size, uint32_t placement,
			      uint32_t flags, uint16_t cpu_caching)
{
	uint32_t handle;

	igt_assert_eq(__xe_bo_create_caching(fd, vm, size, placement, flags,
					     cpu_caching, &handle), 0);

	return handle;
}

uint32_t xe_bind_exec_queue_create(int fd, uint32_t vm, uint64_t ext)
{
	struct drm_xe_engine_class_instance instance = {
		.engine_class = DRM_XE_ENGINE_CLASS_VM_BIND,
	};
	struct drm_xe_exec_queue_create create = {
		.extensions = ext,
		.vm_id = vm,
		.width = 1,
		.num_placements = 1,
		.instances = to_user_pointer(&instance),
	};

	igt_assert_eq(igt_ioctl(fd, DRM_IOCTL_XE_EXEC_QUEUE_CREATE, &create), 0);

	return create.exec_queue_id;
}

int __xe_exec_queue_create(int fd, uint32_t vm, uint16_t width, uint16_t num_placements,
			   struct drm_xe_engine_class_instance *instance,
			   uint64_t ext, uint32_t *exec_queue_id)
{
	struct drm_xe_exec_queue_create create = {
		.extensions = ext,
		.vm_id = vm,
		.width = width,
		.num_placements = num_placements,
		.instances = to_user_pointer(instance),
	};
	int err;

	err = igt_ioctl(fd, DRM_IOCTL_XE_EXEC_QUEUE_CREATE, &create);
	if (err) {
		err = -errno;
		igt_assume(err);
		errno = 0;
		return err;
	}

	*exec_queue_id = create.exec_queue_id;
	return 0;
}

uint32_t xe_exec_queue_create(int fd, uint32_t vm,
			      struct drm_xe_engine_class_instance *instance,
			      uint64_t ext)
{
	uint32_t exec_queue_id;

	igt_assert_eq(__xe_exec_queue_create(fd, vm, 1, 1, instance, ext, &exec_queue_id), 0);

	return exec_queue_id;
}

uint32_t xe_exec_queue_create_class(int fd, uint32_t vm, uint16_t class)
{
	struct drm_xe_engine_class_instance instance = {
		.engine_class = class,
		.engine_instance = 0,
		.gt_id = 0,
	};
	struct drm_xe_exec_queue_create create = {
		.vm_id = vm,
		.width = 1,
		.num_placements = 1,
		.instances = to_user_pointer(&instance),
	};

	igt_assert_eq(igt_ioctl(fd, DRM_IOCTL_XE_EXEC_QUEUE_CREATE, &create), 0);

	return create.exec_queue_id;
}

void xe_exec_queue_destroy(int fd, uint32_t exec_queue)
{
	struct drm_xe_exec_queue_destroy destroy = {
		.exec_queue_id = exec_queue,
	};

	igt_assert_eq(igt_ioctl(fd, DRM_IOCTL_XE_EXEC_QUEUE_DESTROY, &destroy), 0);
}

uint64_t xe_bo_mmap_offset(int fd, uint32_t bo)
{
	struct drm_xe_gem_mmap_offset mmo = {
		.handle = bo,
	};

	igt_assert_eq(igt_ioctl(fd, DRM_IOCTL_XE_GEM_MMAP_OFFSET, &mmo), 0);

	return mmo.offset;
}

static void *__xe_bo_map(int fd, uint16_t bo, size_t size, int prot)
{
	uint64_t mmo;
	void *map;

	mmo = xe_bo_mmap_offset(fd, bo);
	map = mmap(NULL, size, prot, MAP_SHARED, fd, mmo);
	igt_assert(map != MAP_FAILED);

	return map;
}

void *xe_bo_map(int fd, uint32_t bo, size_t size)
{
	return __xe_bo_map(fd, bo, size, PROT_WRITE);
}

void *xe_bo_mmap_ext(int fd, uint32_t bo, size_t size, int prot)
{
	return __xe_bo_map(fd, bo, size, prot);
}

int __xe_exec(int fd, struct drm_xe_exec *exec)
{
	int err = 0;

	if (igt_ioctl(fd, DRM_IOCTL_XE_EXEC, exec)) {
		err = -errno;
		igt_assume(err != 0);
	}
	errno = 0;
	return err;
}

void xe_exec(int fd, struct drm_xe_exec *exec)
{
	igt_assert_eq(__xe_exec(fd, exec), 0);
}

void xe_exec_sync(int fd, uint32_t exec_queue, uint64_t addr,
		  struct drm_xe_sync *sync, uint32_t num_syncs)
{
	struct drm_xe_exec exec = {
		.exec_queue_id = exec_queue,
		.syncs = (uintptr_t)sync,
		.num_syncs = num_syncs,
		.address = addr,
		.num_batch_buffer = 1,
	};

	igt_assert_eq(__xe_exec(fd, &exec), 0);
}

void xe_exec_wait(int fd, uint32_t exec_queue, uint64_t addr)
{
	struct drm_xe_sync sync = {
		.type = DRM_XE_SYNC_TYPE_SYNCOBJ,
		.flags = DRM_XE_SYNC_FLAG_SIGNAL,
		.handle = syncobj_create(fd, 0),
	};

	xe_exec_sync(fd, exec_queue, addr, &sync, 1);

	igt_assert(syncobj_wait(fd, &sync.handle, 1, INT64_MAX, 0, NULL));
	syncobj_destroy(fd, sync.handle);
}

/**
 * __xe_wait_ufence:
 * @fd: xe device fd
 * @addr: address of value to compare
 * @value: expected value (equal) in @address
 * @exec_queue: exec_queue id
 * @timeout: pointer to time to wait in nanoseconds
 *
 * Function compares @value with memory pointed by @addr until they are equal.
 *
 * Returns (in @timeout), the elapsed time in nanoseconds if user fence was
 * signalled. Returns 0 on success, -errno of ioctl on error.
 */
int __xe_wait_ufence(int fd, uint64_t *addr, uint64_t value,
		     uint32_t exec_queue, int64_t *timeout)
{
	struct drm_xe_wait_user_fence wait = {
		.addr = to_user_pointer(addr),
		.op = DRM_XE_UFENCE_WAIT_OP_EQ,
		.flags = 0,
		.value = value,
		.mask = DRM_XE_UFENCE_WAIT_MASK_U64,
		.exec_queue_id = exec_queue,
	};

	igt_assert(timeout);
	wait.timeout = *timeout;

	if (igt_ioctl(fd, DRM_IOCTL_XE_WAIT_USER_FENCE, &wait))
		return -errno;

	*timeout = wait.timeout;
	return 0;
}

/**
 * xe_wait_ufence:
 * @fd: xe device fd
 * @addr: address of value to compare
 * @value: expected value (equal) in @address
 * @exec_queue: exec_queue id
 * @timeout: time to wait in nanoseconds
 *
 * Function compares @value with memory pointed by @addr until they are equal.
 * Asserts that ioctl returned without error.
 *
 * Returns elapsed time in nanoseconds if user fence was signalled.
 */
int64_t xe_wait_ufence(int fd, uint64_t *addr, uint64_t value,
		       uint32_t exec_queue, int64_t timeout)
{
	igt_assert_eq(__xe_wait_ufence(fd, addr, value, exec_queue, &timeout), 0);
	return timeout;
}

void xe_force_gt_reset(int fd, int gt)
{
	char reset_string[128];
	struct stat st;

	igt_assert_eq(fstat(fd, &st), 0);

	snprintf(reset_string, sizeof(reset_string),
		 "cat /sys/kernel/debug/dri/%d/gt%d/force_reset",
		 minor(st.st_rdev), gt);
	system(reset_string);
}

static void xe_oa_prop_to_ext(struct drm_xe_oa_open_prop *properties,
			      struct drm_xe_ext_set_property *extn)
{
	__u64 *prop = (__u64 *)properties->properties_ptr;
	struct drm_xe_ext_set_property *ext = extn;
	int i, j;

	for (i = 0; i < properties->num_properties; i++) {
		ext->base.name = DRM_XE_OA_EXTENSION_SET_PROPERTY;
		ext->property = *prop++;
		ext->value = *prop++;
		ext++;
	}

	if (properties->flags) {
		ext->base.name = DRM_XE_OA_EXTENSION_SET_PROPERTY;
		ext->property = DRM_XE_OA_PROPERTY_OPEN_FLAGS;
		ext->value = properties->flags;
		ext++;
		i++;
	}

	igt_assert_lte(1, i);
	ext = extn;
	for (j = 0; j < i - 1; j++)
		ext[j].base.next_extension = (__u64)&ext[j + 1];
}

int xe_perf_ioctl(int fd, unsigned long request,
		  enum drm_xe_perf_op op, void *arg)
{
#define XE_OA_MAX_SET_PROPERTIES 16

	struct drm_xe_ext_set_property ext[XE_OA_MAX_SET_PROPERTIES] = {};

	/* Chain the PERF layer struct */
	struct drm_xe_perf_param p = {
		.extensions = 0,
		.perf_type = DRM_XE_PERF_TYPE_OA,
		.perf_op = op,
		.param = (__u64)((op == DRM_XE_PERF_OP_STREAM_OPEN) ? ext : arg),
	};

	if (op == DRM_XE_PERF_OP_STREAM_OPEN) {
		struct drm_xe_oa_open_prop *oprop = (struct drm_xe_oa_open_prop *)arg;

		igt_assert_lte(oprop->num_properties, XE_OA_MAX_SET_PROPERTIES);
		xe_oa_prop_to_ext(oprop, ext);
	}

	return igt_ioctl(fd, request, &p);
}

void xe_perf_ioctl_err(int fd, unsigned long request,
		       enum drm_xe_perf_op op, void *arg, int err)
{
	igt_assert_eq(xe_perf_ioctl(fd, request, op, arg), -1);
	igt_assert_eq(errno, err);
	errno = 0;
}
