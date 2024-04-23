// SPDX-License-Identifier: MIT
/*
 * Copyright © 2024 Intel Corporation
 */

/**
 * TEST: cause fake gt reset failure which put Xe device in wedged state
 * Category: Software building block
 * Sub-category: driver
 * Functionality: wedged
 * Test category: functionality test
 */

#include <limits.h>
#include <dirent.h>

#include "igt.h"
#include "igt_device.h"
#include "igt_kmod.h"
#include "igt_sysfs.h"

#include "xe/xe_ioctl.h"

static void force_wedged(int fd)
{
	igt_debugfs_write(fd, "fail_gt_reset/probability", "100");
	igt_debugfs_write(fd, "fail_gt_reset/times", "2");

	xe_force_gt_reset(fd, 0);
	sleep(1);
}

static int rebind_xe(int fd)
{
	char pci_slot[NAME_MAX];
	int sysfs;

	igt_device_get_pci_slot_name(fd, pci_slot);

	sysfs = open("/sys/bus/pci/drivers/xe", O_DIRECTORY);
	igt_assert(sysfs);

        igt_assert(igt_sysfs_set(sysfs, "unbind", pci_slot));

	/*
	 * We need to close the client for a proper release, before
	 * binding back again.
	 */
	close(fd);

        igt_assert(igt_sysfs_set(sysfs, "bind", pci_slot));
	close(sysfs);

	/* Renew the client connection */
	fd = drm_open_driver(DRIVER_XE);
	igt_assert(fd);

	return fd;
}

static int simple_ioctl(int fd)
{
	int ret;

	struct drm_xe_vm_create create = {
		.extensions = 0,
		.flags = 0,
	};

	ret = igt_ioctl(fd, DRM_IOCTL_XE_VM_CREATE, &create);

	if (ret == 0)
		xe_vm_destroy(fd, create.vm_id);

	return ret;
}

/**
 * SUBTEST: basic-wedged
 * Description: Force Xe device wedged after injecting a failure in GT reset
 */
igt_main
{
	int fd;

	igt_fixture {
		fd = drm_open_driver(DRIVER_XE);
	}

	igt_subtest("basic-wedged") {
		igt_require(igt_debugfs_exists(fd, "fail_gt_reset/probability",
					       O_RDWR));

		igt_assert_eq(simple_ioctl(fd), 0);
		force_wedged(fd);
		igt_assert_neq(simple_ioctl(fd), 0);
		fd = rebind_xe(fd);
		igt_assert_eq(simple_ioctl(fd), 0);
	}

	igt_fixture {
		if (igt_debugfs_exists(fd, "fail_gt_reset/probability", O_RDWR)) {
			igt_debugfs_write(fd, "fail_gt_reset/probability", "0");
			igt_debugfs_write(fd, "fail_gt_reset/times", "1");
		}
		drm_close_driver(fd);
	}
}
