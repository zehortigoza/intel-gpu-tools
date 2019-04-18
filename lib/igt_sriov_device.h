/* SPDX-License-Identifier: MIT */
/*
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 */

#ifndef __IGT_SRIOV_DEVICE_H__
#define __IGT_SRIOV_DEVICE_H__

/* Library for managing SR-IOV (Single Root I/O Virtualization)
 * devices.
 *
 * SR-IOV is a specification that allows a single PCIe physical
 * device to appear as a physical function (PF) and multiple virtual
 * functions (VFs) to the operating system.
 */

bool igt_sriov_is_pf(int device);
bool igt_sriov_vfs_supported(int pf);
unsigned int igt_sriov_get_total_vfs(int pf);
unsigned int igt_sriov_get_enabled_vfs(int pf);
void igt_sriov_enable_vfs(int pf, unsigned int num_vfs);
void igt_sriov_disable_vfs(int pf);
bool igt_sriov_is_driver_autoprobe_enabled(int pf);
void igt_sriov_enable_driver_autoprobe(int pf);
void igt_sriov_disable_driver_autoprobe(int pf);

#endif /* __IGT_SRIOV_DEVICE_H__ */
