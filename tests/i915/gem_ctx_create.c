/*
 * Copyright © 2012 Intel Corporation
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice (including the next
 * paragraph) shall be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
 * IN THE SOFTWARE.
 *
 * Authors:
 *    Ben Widawsky <ben@bwidawsk.net>
 *
 */

#include "igt.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <time.h>

#include "igt_rand.h"

#define LOCAL_I915_EXEC_BSD_SHIFT      (13)
#define LOCAL_I915_EXEC_BSD_MASK       (3 << LOCAL_I915_EXEC_BSD_SHIFT)

#define ENGINE_FLAGS  (I915_EXEC_RING_MASK | LOCAL_I915_EXEC_BSD_MASK)

static unsigned all_engines[16];
static unsigned all_nengine;

static unsigned ppgtt_engines[16];
static unsigned ppgtt_nengine;

static int __gem_context_create_local(int fd, struct drm_i915_gem_context_create *arg)
{
	int ret = 0;
	if (drmIoctl(fd, DRM_IOCTL_I915_GEM_CONTEXT_CREATE, arg))
		ret = -errno;
	return ret;
}

static double elapsed(const struct timespec *start,
		      const struct timespec *end)
{
	return (end->tv_sec - start->tv_sec) + 1e-9*(end->tv_nsec - start->tv_nsec);
}

static void files(int core, int timeout, const int ncpus)
{
	const uint32_t bbe = MI_BATCH_BUFFER_END;
	struct drm_i915_gem_execbuffer2 execbuf;
	struct drm_i915_gem_exec_object2 obj;
	uint32_t batch, name;

	batch = gem_create(core, 4096);
	gem_write(core, batch, 0, &bbe, sizeof(bbe));
	name = gem_flink(core, batch);

	memset(&obj, 0, sizeof(obj));
	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(&obj);
	execbuf.buffer_count = 1;

	igt_fork(child, ncpus) {
		struct timespec start, end;
		unsigned count = 0;

		clock_gettime(CLOCK_MONOTONIC, &start);
		do {
			do {
				int fd = drm_open_driver(DRIVER_INTEL);
				obj.handle = gem_open(fd, name);
				execbuf.flags &= ~ENGINE_FLAGS;
				execbuf.flags |= ppgtt_engines[count % ppgtt_nengine];
				gem_execbuf(fd, &execbuf);
				close(fd);
			} while (++count & 1023);
			clock_gettime(CLOCK_MONOTONIC, &end);
		} while (elapsed(&start, &end) < timeout);

		gem_sync(core, batch);
		clock_gettime(CLOCK_MONOTONIC, &end);
		igt_info("[%d] File creation + execution: %.3f us\n",
			 child, elapsed(&start, &end) / count *1e6);
	}
	igt_waitchildren();

	gem_close(core, batch);
}

static void active(int fd, unsigned engine, int timeout, int ncpus)
{
	const uint32_t bbe = MI_BATCH_BUFFER_END;
	struct drm_i915_gem_execbuffer2 execbuf;
	struct drm_i915_gem_exec_object2 obj;
	unsigned int nengine, engines[16];
	unsigned *shared;

	if (engine == ALL_ENGINES) {
		igt_require(all_nengine);
		nengine = all_nengine;
		memcpy(engines, all_engines, sizeof(engines[0])*nengine);
	} else {
		gem_require_ring(fd, engine);
		nengine = 1;
		engines[0] = engine;
	}

	shared = mmap(NULL, 4096, PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
	igt_assert(shared != MAP_FAILED);

	memset(&obj, 0, sizeof(obj));
	obj.handle = gem_create(fd, 4096);
	gem_write(fd, obj.handle, 0, &bbe, sizeof(bbe));

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(&obj);
	execbuf.buffer_count = 1;

	if (ncpus < 0) {
		igt_fork(child, ppgtt_nengine) {
			unsigned long count = 0;

			if (ppgtt_engines[child] == engine)
				continue;

			execbuf.flags = ppgtt_engines[child];

			while (!*(volatile unsigned *)shared) {
				obj.handle = gem_create(fd, 4096 << 10);
				gem_write(fd, obj.handle, 0, &bbe, sizeof(bbe));

				gem_execbuf(fd, &execbuf);
				gem_close(fd, obj.handle);
				count++;
			}

			igt_debug("hog[%d]: cycles=%lu\n", child, count);
		}
		ncpus = -ncpus;
	}

	igt_fork(child, ncpus) {
		struct timespec start, end;
		unsigned count = 0;

		clock_gettime(CLOCK_MONOTONIC, &start);
		do {
			do {
				execbuf.rsvd1 = gem_context_create(fd);
				for (unsigned n = 0; n < nengine; n++) {
					execbuf.flags = engines[n];
					gem_execbuf(fd, &execbuf);
				}
				gem_context_destroy(fd, execbuf.rsvd1);
			} while (++count & 1023);
			clock_gettime(CLOCK_MONOTONIC, &end);
		} while (elapsed(&start, &end) < timeout);

		gem_sync(fd, obj.handle);
		clock_gettime(CLOCK_MONOTONIC, &end);
		igt_info("[%d] Context creation + execution: %.3f us\n",
			 child, elapsed(&start, &end) / count *1e6);

		shared[0] = 1;
	}
	igt_waitchildren();

	gem_close(fd, obj.handle);
	munmap(shared, 4096);
}

static void xchg_u32(void *array, unsigned i, unsigned j)
{
	uint32_t *a = array, tmp;

	tmp = a[i];
	a[i] = a[j];
	a[j] = tmp;
}

static unsigned __context_size(int fd)
{
	switch (intel_gen(intel_get_drm_devid(fd))) {
	case 0:
	case 1:
	case 2:
	case 3:
	case 4:
	case 5:
	case 6:
	case 7: return 17 << 12;
	case 8: return 20 << 12;
	case 9: return 22 << 12;
	default: return 32 << 12;
	}
}

static unsigned context_size(int fd)
{
	uint64_t size;

	size = __context_size(fd);
	if (ppgtt_nengine > 1) {
		size += 4 << 12; /* ringbuffer as well */
		size *= ppgtt_nengine;
	}

	return size;
}

static uint64_t total_avail_mem(unsigned mode)
{
	uint64_t total = intel_get_avail_ram_mb();
	if (mode & CHECK_SWAP)
		total += intel_get_total_swap_mb();
	return total << 20;
}

static void maximum(int fd, int ncpus, unsigned mode)
{
	const uint32_t bbe = MI_BATCH_BUFFER_END;
	struct drm_i915_gem_execbuffer2 execbuf;
	struct drm_i915_gem_exec_object2 obj[2];
	uint64_t avail_mem = total_avail_mem(mode);
	unsigned ctx_size = context_size(fd);
	uint32_t *contexts = NULL;
	unsigned long count = 0;
	uint32_t ctx_id;

	do {
		int err;

		if ((count & -count) == count) {
			int sz = count ? 2*count : 1;
			contexts = realloc(contexts,
					   sz*sizeof(*contexts));
			igt_assert(contexts);
		}

		err = -ENOMEM;
		if (avail_mem > (count + 1) * ctx_size)
			err = __gem_context_create(fd, &ctx_id);
		if (err) {
			igt_info("Created %lu contexts, before failing with '%s' [%d]\n",
				 count, strerror(-err), -err);
			break;
		}

		contexts[count++] = ctx_id;
	} while (1);
	igt_require(count);

	memset(obj, 0, sizeof(obj));
	obj[1].handle = gem_create(fd, 4096);
	gem_write(fd, obj[1].handle, 0, &bbe, sizeof(bbe));

	memset(&execbuf, 0, sizeof(execbuf));
	execbuf.buffers_ptr = to_user_pointer(obj);
	execbuf.buffer_count = 2;

	igt_fork(child, ncpus) {
		struct timespec start, end;

		hars_petruska_f54_1_random_perturb(child);
		obj[0].handle = gem_create(fd, 4096);

		clock_gettime(CLOCK_MONOTONIC, &start);
		for (int repeat = 0; repeat < 3; repeat++) {
			igt_permute_array(contexts, count, xchg_u32);
			igt_permute_array(all_engines, all_nengine, xchg_u32);

			for (unsigned long i = 0; i < count; i++) {
				execbuf.rsvd1 = contexts[i];
				for (unsigned long j = 0; j < all_nengine; j++) {
					execbuf.flags = all_engines[j];
					gem_execbuf(fd, &execbuf);
				}
			}
		}
		gem_sync(fd, obj[0].handle);
		clock_gettime(CLOCK_MONOTONIC, &end);
		gem_close(fd, obj[0].handle);

		igt_info("[%d] Context execution: %.3f us\n", child,
			 elapsed(&start, &end) / (3 * count * all_nengine) * 1e6);
	}
	igt_waitchildren();

	gem_close(fd, obj[1].handle);

	for (unsigned long i = 0; i < count; i++)
		gem_context_destroy(fd, contexts[i]);
	free(contexts);
}

igt_main
{
	const int ncpus = sysconf(_SC_NPROCESSORS_ONLN);
	struct drm_i915_gem_context_create create;
	int fd = -1;

	igt_fixture {
		const struct intel_execution_engine *exec_engine_iter;
		unsigned engine;

		fd = drm_open_driver(DRIVER_INTEL);
		igt_require_gem(fd);
		gem_require_contexts(fd);

		for_each_physical_engine(fd, exec_engine_iter, engine)
			all_engines[all_nengine++] = engine;
		igt_require(all_nengine);

		if (gem_uses_full_ppgtt(fd)) {
			ppgtt_nengine = all_nengine;
			memcpy(ppgtt_engines,
			       all_engines,
			       all_nengine * sizeof(all_engines[0]));
		} else
			ppgtt_engines[ppgtt_nengine++] = 0;

		igt_fork_hang_detector(fd);
	}

	igt_subtest("basic") {
		memset(&create, 0, sizeof(create));
		create.ctx_id = rand();
		create.pad = 0;
		igt_assert_eq(__gem_context_create_local(fd, &create), 0);
		igt_assert(create.ctx_id != 0);
		gem_context_destroy(fd, create.ctx_id);
	}

	igt_subtest("invalid-pad") {
		memset(&create, 0, sizeof(create));
		create.ctx_id = rand();
		create.pad = 1;
		igt_assert_eq(__gem_context_create_local(fd, &create), -EINVAL);
	}

	igt_subtest("maximum-mem")
		maximum(fd, ncpus, CHECK_RAM);
	igt_subtest("maximum-swap")
		maximum(fd, ncpus, CHECK_RAM | CHECK_SWAP);

	igt_subtest("basic-files")
		files(fd, 5, 1);
	igt_subtest("files")
		files(fd, 150, 1);
	igt_subtest("forked-files")
		files(fd, 150, ncpus);

	igt_subtest("active-all")
		active(fd, ALL_ENGINES, 120, 1);
	igt_subtest("forked-active-all")
		active(fd, ALL_ENGINES, 120, ncpus);

	for (const struct intel_execution_engine *e = intel_execution_engines;
	     e->name; e++) {
		igt_subtest_f("active-%s", e->name)
			active(fd, e->exec_id | e->flags, 20, 1);
		igt_subtest_f("forked-active-%s", e->name)
			active(fd, e->exec_id | e->flags, 20, ncpus);
		if (e->exec_id) {
			igt_subtest_f("hog-%s", e->name)
				active(fd, e->exec_id | e->flags, 20, -1);
		}
	}

	igt_fixture {
		igt_stop_hang_detector();
		close(fd);
	}
}
