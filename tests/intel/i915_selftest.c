/*
 * Copyright © 2016 Intel Corporation
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
 */

#include "igt.h"
#include "igt_kmod.h"
/**
 * TEST: i915 selftest
 * Description: Basic unit tests for i915.ko
 *
 * SUBTEST: live
 * Category: Selftest
 * Sub-category: Selftest subcategory
 * Functionality: live selftest
 * Feature: gem_core
 *
 * SUBTEST: live@active
 * Category: Selftest
 * Functionality: semaphore
 * Test category: i915
 * Sub-category: Synchronization
 *
 * SUBTEST: live@blt
 * Category: Selftest
 * Description: Blitter validation
 * Functionality: command streamer
 * Test category: i915 / HW
 * Sub-category: Blitter
 *
 * SUBTEST: live@client
 * Category: Selftest
 * Description: Internal API over blitter
 * Functionality: blitter api
 * Test category: i915
 * Sub-category: uapi
 *
 * SUBTEST: live@coherency
 * Category: Selftest
 * Description: Cache management
 * Functionality: cache
 * Test category: i915 / HW
 * Sub-category: Memory management
 *
 * SUBTEST: live@debugger
 * Category: Selftest
 * Functionality: device management
 * Test category: debugger
 * Sub-category: Debugging
 *
 * SUBTEST: live@display
 * Category: Selftest
 * Functionality: display sanity
 * Test category: i915
 * Sub-category: Display tests
 *
 * SUBTEST: live@dmabuf
 * Category: Selftest
 * Functionality: buffer management
 * Test category: i915
 * Sub-category: Memory management
 *
 * SUBTEST: live@evict
 * Category: Selftest
 * Functionality: GTT eviction
 * Test category: i915
 * Sub-category: Memory management
 *
 * SUBTEST: live@execlists
 * Category: Selftest
 * Description: command submission backend
 * Functionality: execlists
 * Test category: i915
 * Sub-category: CMD submission
 *
 * SUBTEST: live@gem
 * Category: Selftest
 * Functionality: execbuf
 * Test category: i915
 * Sub-category: CMD submission
 *
 * SUBTEST: live@gem_contexts
 * Category: Selftest
 * Description: User isolation and execution at the context level
 * Functionality: context
 * Sub-category: CMD submission
 * Test category: GEM_Legacy
 * Test category: i915 / HW
 *
 * SUBTEST: live@gem_execbuf
 * Category: Selftest
 * Description: command submission support
 * Functionality: execbuf
 * Test category: i915
 * Sub-category: CMD submission
 *
 * SUBTEST: live@gt_ccs_mode
 * Category: Selftest
 * Description: Multi-ccs internal validation
 * Functionality: multii-ccs
 * Test category: i915
 * Sub-category: CMD submission
 *
 * SUBTEST: live@gt_contexts
 * Category: Selftest
 * Description: HW isolation and HW context validation
 * Functionality: context
 * Sub-category: CMD submission
 * Test category: HW
 *
 * SUBTEST: live@gt_engines
 * Category: Selftest
 * Description: command submission topology validation
 * Functionality: command submission topology
 * Test category: i915
 * Sub-category: CMD submission
 *
 * SUBTEST: live@gt_gtt
 * Category: Selftest
 * Description: Validation of virtual address management and execution
 * Functionality: gtt
 * Test category: HW
 * Sub-category: Memory management
 *
 * SUBTEST: live@gt_heartbeat
 * Category: Selftest
 * Description: Stall detection interface validation
 * Functionality: heartbeat
 * Test category: i915
 * Sub-category: Reset
 *
 * SUBTEST: live@gt_lrc
 * Category: Selftest
 * Description: HW isolation and HW context validation
 * Functionality: context
 * Sub-category: CMD submission
 * Test category: HW
 *
 * SUBTEST: live@gt_mocs
 * Category: Selftest
 * Description: Verification of mocs registers
 * Functionality: mocs registers
 * Test category: i915 / HW
 * Sub-category: Mocs
 *
 * SUBTEST: live@gt_pm
 * Category: Selftest
 * Description: Basic i915 driver module selftests
 * Functionality: rps, rc6
 * Sub-category: Power management
 *
 * SUBTEST: live@gt_timelines
 * Category: Selftest
 * Description: semaphore tracking
 * Functionality: semaphore
 * Test category: i915
 * Sub-category: Synchronization
 *
 * SUBTEST: live@gt_tlb
 * Category: Selftest
 * Test category: Memory Management
 * Sub-category: Memory management
 * Functionality: tlb
 *
 * SUBTEST: live@gtt
 * Category: Selftest
 * Description: Virtual address management interface validation
 * Functionality: virtual address
 * Test category: i915
 * Sub-category: Memory management
 *
 * SUBTEST: live@gtt_l4wa
 * Category: Selftest
 * Description: Check the L4WA is enabled when it was required
 * Functionality: L4WA
 * Test category: i915
 * Sub-category: Workarounds
 *
 * SUBTEST: live@guc
 * Category: Selftest
 * Feature: firmware feature
 * Sub-category: Firmware
 * Functionality: GUC
 * Test category: GuC
 *
 * SUBTEST: live@guc_doorbells
 * Category: Selftest
 * Feature: firmware feature
 * Sub-category: Firmware
 * Functionality: GUC
 * Test category: GuC
 *
 * SUBTEST: live@guc_hang
 * Category: Selftest
 * Feature: firmware feature
 * Sub-category: Firmware
 * Functionality: GUC
 * Test category: GuC
 *
 * SUBTEST: live@guc_multi_lrc
 * Category: Selftest
 * Feature: firmware feature
 * Sub-category: Firmware
 * Functionality: GUC
 * Test category: GuC
 *
 * SUBTEST: live@hangcheck
 * Category: Selftest
 * Description: reset handling after stall detection
 * Functionality: hangcheck
 * Test category: i915
 * Sub-category: Reset
 *
 * SUBTEST: live@hugepages
 * Category: Selftest
 * Description: Large page support validation
 * Functionality: large page
 * Test category: i915
 * Sub-category: Memory management
 *
 * SUBTEST: live@late_gt_pm
 * Category: Selftest
 * Feature: rc6 feature
 * Functionality: Basic i915 driver module selftests
 * Test category: rc6
 * Sub-category: Power management
 *
 * SUBTEST: live@lmem
 * Category: Selftest
 * Feature: local_memory
 * Description: Basic i915 driver module selftests
 * Functionality: local memory
 * Sub-category: Memory management
 *
 * SUBTEST: live@memory_region
 * Category: Selftest
 * Description: memory topology validation and migration checks
 * Functionality: memory topology
 * Test category: i915 / HW
 * Sub-category: Memory management
 *
 * SUBTEST: live@memory_region_cross_tile
 * Category: Selftest
 * Description: Multi-tile memory topology validation
 * Functionality: memory topology
 * Sub-category: MultiTile
 *
 * SUBTEST: live@mman
 * Category: Selftest
 * Description: memory management validation
 * Functionality: mapping
 * Test category: i915
 * Sub-category: Memory management
 *
 * SUBTEST: live@obj_lock
 * Category: Selftest
 * Description: Validation of per-object locking patterns
 * Functionality: per-object locking
 * Test category: i915
 * Sub-category: Core
 *
 * SUBTEST: live@objects
 * Category: Selftest
 * Description: User object allocation and isolation checks
 * Functionality: buffer management
 * Test category: i915
 * Sub-category: Core
 *
 * SUBTEST: live@perf
 * Category: Selftest
 * Feature: i915 perf selftests
 * Description: Basic i915 module perf unit selftests
 * Functionality: perf
 * Sub-category: Performance
 *
 * SUBTEST: live@remote_tiles
 * Category: Selftest
 * Description: Tile meta data validation
 * Functionality: meta data
 * Sub-category: MultiTile
 *
 * SUBTEST: live@requests
 * Category: Selftest
 * Description: Validation of internal i915 command submission interface
 * Functionality: command submission interface
 * Test category: i915
 * Sub-category: CMD submission
 *
 * SUBTEST: live@reset
 * Category: Selftest
 * Description: engine/GT resets
 * Functionality: engine/GT reset
 * Test category: HW
 * Sub-category: Reset
 *
 * SUBTEST: live@sanitycheck
 * Description: Checks the selftest infrastructure itself
 * Category: Selftest
 * Sub-category: Core
 * Functionality: sanitycheck
 * Test category: i915
 *
 * SUBTEST: live@scheduler
 * Category: Selftest
 * Sub-category: CMD Submission
 * Functionality: scheduler
 *
 * SUBTEST: live@semaphores
 * Category: Selftest
 * Description: GuC semaphore management
 * Functionality: semaphore
 * Test category: HW
 * Sub-category: Synchronization
 *
 * SUBTEST: live@slpc
 * Category: Selftest
 * Feature: slpc feature/ pm_rps
 * Test category: slpc / pm_rps
 * Description: Basic i915 driver module selftests
 * Functionality: slpc
 * Sub-category: Power management
 *
 * SUBTEST: live@uncore
 * Category: Selftest
 * Description: Basic i915 driver module selftests
 * Feature: forcewake feature
 * Test category: forcewake
 * Functionality: forcewake
 * Sub-category: Power management
 *
 * SUBTEST: live@vma
 * Category: Selftest
 * Description: Per-object virtual address management
 * Functionality: virtual address
 * Test category: i915
 * Sub-category: Memory management
 *
 * SUBTEST: live@win_blt_copy
 * Category: Selftest
 * Description: Validation of migration interface
 * Functionality: migration interface
 * Test category: i915 / HW
 * Sub-category: Blitter
 *
 * SUBTEST: live@workarounds
 * Category: Selftest
 * Description: Check workarounds persist or are reapplied after resets and other power management events
 * Functionality: driver workarounds
 * Test category: HW
 * Sub-category: Workarounds
 *
 * SUBTEST: mock
 * Category: Selftest
 * Sub-category: Selftest subcategory
 * Functionality: mock selftest
 * Feature: gem_core
 *
 * SUBTEST: mock@buddy
 * Category: Selftest
 * Description: Buddy allocation
 * Functionality: buddy allocation
 * Test category: DRM
 * Sub-category: Memory management
 *
 * SUBTEST: mock@contexts
 * Category: Selftest
 * Description: GEM context internal API checks
 * Functionality: context
 * Test category: i915
 * Sub-category: uapi
 *
 * SUBTEST: mock@dmabuf
 * Category: Selftest
 * Description: dma-buf (buffer management) API checks
 * Functionality: buffer management
 * Test category: DRM
 * Sub-category: uapi
 *
 * SUBTEST: mock@engine
 * Category: Selftest
 * Description: Engine topology API checks
 * Functionality: engine topology
 * Test category: i915
 * Sub-category: uapi
 *
 * SUBTEST: mock@evict
 * Category: Selftest
 * Description: GTT eviction API checks
 * Functionality: gtt eviction
 * Test category: i915
 * Sub-category: uapi
 *
 * SUBTEST: mock@fence
 * Category: Selftest
 * Description: semaphore API checks
 * Functionality: semaphore
 * Test category: i915
 * Sub-category: uapi
 *
 * SUBTEST: mock@gtt
 * Category: Selftest
 * Description: Virtual address management API checks
 * Functionality: gtt
 * Test category: i915
 * Sub-category: uapi
 *
 * SUBTEST: mock@hugepages
 * Category: Selftest
 * Description: Hugepage API checks
 * Functionality: huge page
 * Test category: i915
 * Sub-category: uapi
 *
 * SUBTEST: mock@memory_region
 * Category: Selftest
 * Description: Memory region API checks
 * Functionality: memory region
 * Test category: i915
 * Sub-category: uapi
 *
 * SUBTEST: mock@objects
 * Category: Selftest
 * Description: Buffer object API checks
 * Functionality: buffer object
 * Test category: i915
 * Sub-category: uapi
 *
 * SUBTEST: mock@phys
 * Category: Selftest
 * Description: legacy physical object API checks
 * Functionality: physical object
 * Test category: i915
 * Sub-category: uapi
 *
 * SUBTEST: mock@requests
 * Category: Selftest
 * Description: Internal command submission API checks
 * Functionality: requests
 * Test category: i915
 * Sub-category: uapi
 *
 * SUBTEST: mock@ring
 * Category: Selftest
 * Description: Ringbuffer management API checks
 * Functionality: ringbuffer
 * Test category: i915
 * Sub-category: uapi
 *
 * SUBTEST: mock@sanitycheck
 * Description: Selftest for the selftest
 * Category: Selftest
 * Sub-category: Core
 * Functionality: sanitycheck
 * Test category: i915
 *
 * SUBTEST: mock@scatterlist
 * Category: Selftest
 * Description: Scatterlist API checks
 * Functionality: scatterlist
 * Test category: i915
 * Sub-category: uapi
 *
 * SUBTEST: mock@shmem
 * Category: Selftest
 * Description: SHM utils API checks
 * Functionality: shm
 * Test category: i915
 * Sub-category: uapi
 *
 * SUBTEST: mock@syncmap
 * Category: Selftest
 * Description: API checks for the contracted radixtree
 * Functionality: contracted radixtree
 * Test category: i915
 * Sub-category: uapi
 *
 * SUBTEST: mock@timelines
 * Category: Selftest
 * Description: API checks for semaphore tracking
 * Functionality: semaphore
 * Test category: i915
 * Sub-category: uapi
 *
 * SUBTEST: mock@tlb
 * Category: Selftest
 * Test category: Memory Management
 * Sub-category: Memory management
 * Functionality: tlb
 *
 * SUBTEST: mock@uncore
 * Category: Selftest
 * Description: Basic i915 driver module selftests
 * Feature: forcewake feature
 * Test category: forcewake
 * Functionality: forcewake
 * Sub-category: Power management
 *
 * SUBTEST: mock@vma
 * Category: Selftest
 * Description: API checks for virtual address management
 * Functionality: virtual address
 * Test category: i915
 * Sub-category: uapi
 *
 * SUBTEST: perf
 * Category: Selftest
 * Feature: i915 perf selftests
 * Functionality: oa
 * Sub-category: Performance
 *
 * SUBTEST: perf@blt
 * Category: Selftest
 * Feature: i915 perf selftests
 * Description: Basic i915 module perf unit selftests
 * Functionality: perf
 * Sub-category: Performance
 *
 * SUBTEST: perf@engine_cs
 * Category: Selftest
 * Feature: i915 perf selftests
 * Description: Basic i915 module perf unit selftests
 * Functionality: perf
 * Sub-category: Performance
 *
 * SUBTEST: perf@region
 * Category: Selftest
 * Feature: i915 perf selftests
 * Description: Basic i915 module perf unit selftests
 * Functionality: perf
 * Sub-category: Performance
 *
 * SUBTEST: perf@request
 * Category: Selftest
 * Description: Basic i915 module perf unit selftests
 * Functionality: perf
 * Sub-category: Performance
 *
 * SUBTEST: perf@scheduler
 * Category: Selftest
 * Description: Basic i915 module perf unit selftests
 * Functionality: perf
 * Sub-category: Performance
 */

IGT_TEST_DESCRIPTION("Basic unit tests for i915.ko");

igt_main
{
	const char *env = getenv("SELFTESTS") ?: "";
	char opts[1024];

	igt_assert(snprintf(opts, sizeof(opts),
			    "mock_selftests=-1 disable_display=1 st_filter=%s",
			    env) < sizeof(opts));
	igt_kselftests("i915", opts, NULL, "mock");

	igt_assert(snprintf(opts, sizeof(opts),
			    "live_selftests=-1 disable_display=1 st_filter=%s",
			    env) < sizeof(opts));
	igt_kselftests("i915", opts, "live_selftests", "live");

	igt_assert(snprintf(opts, sizeof(opts),
			    "perf_selftests=-1 disable_display=1 st_filter=%s",
			    env) < sizeof(opts));
	igt_kselftests("i915", opts, "perf_selftests", "perf");
}
