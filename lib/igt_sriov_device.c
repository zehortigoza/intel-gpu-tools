// SPDX-License-Identifier: MIT
/*
 * Copyright(c) 2023 Intel Corporation. All rights reserved.
 */

#include <dirent.h>
#include <errno.h>
#include <pciaccess.h>

#include "drmtest.h"
#include "igt_core.h"
#include "igt_device.h"
#include "igt_sriov_device.h"
#include "igt_sysfs.h"

/**
 * igt_sriov_is_pf - Check if device is PF
 * @device: device file descriptor
 *
 * Determine if device is PF by checking existence of sriov_totalvfs file.
 *
 * Return:
 * True if device is PF, false otherwise.
 */
bool igt_sriov_is_pf(int device)
{
	int sysfs;
	bool ret;

	sysfs = igt_sysfs_open(device);
	igt_assert_fd(sysfs);

	ret = igt_sysfs_has_attr(sysfs, "device/sriov_totalvfs");
	close(sysfs);

	return ret;
}

static bool __pf_attr_get_u32(int pf, const char *attr, uint32_t *value)
{
	int sysfs;
	bool ret;

	igt_assert(igt_sriov_is_pf(pf));

	sysfs = igt_sysfs_open(pf);
	igt_assert_fd(sysfs);

	ret = __igt_sysfs_get_u32(sysfs, attr, value);
	close(sysfs);

	return ret;
}

static uint32_t pf_attr_get_u32(int pf, const char *attr)
{
	uint32_t value;

	igt_assert_f(__pf_attr_get_u32(pf, attr, &value),
		     "Failed to read %s attribute (%s)\n", attr, strerror(errno));

	return value;
}

static bool __pf_attr_set_u32(int pf, const char *attr, uint32_t value)
{
	int sysfs;
	bool ret;

	igt_assert(igt_sriov_is_pf(pf));

	sysfs = igt_sysfs_open(pf);
	igt_assert_fd(sysfs);

	ret = __igt_sysfs_set_u32(sysfs, attr, value);
	close(sysfs);

	return ret;
}

static void pf_attr_set_u32(int pf, const char *attr, uint32_t value)
{
	igt_assert_f(__pf_attr_set_u32(pf, attr, value),
		     "Failed to write %u to %s attribute (%s)\n", value, attr, strerror(errno));
}

/**
 * igt_sriov_vfs_supported - Check if VFs are supported
 * @pf: PF device file descriptor
 *
 * Determine VFs support by checking if value of sriov_totalvfs attribute
 * corresponding to @pf device is bigger than 0.
 *
 * Return:
 * True if VFs are supported, false otherwise.
 */
bool igt_sriov_vfs_supported(int pf)
{
	uint32_t totalvfs;

	if (!__pf_attr_get_u32(pf, "device/sriov_totalvfs", &totalvfs))
		return false;

	return totalvfs > 0;
}

/**
 * igt_sriov_get_totalvfs - Get maximum number of VFs that can be enabled
 * @pf: PF device file descriptor
 *
 * Maximum number of VFs that can be enabled is checked by reading
 * sriov_totalvfs attribute corresponding to @pf device.
 *
 * It asserts on failure.
 *
 * Return:
 * Maximum number of VFs that can be associated with given PF.
 */
unsigned int igt_sriov_get_total_vfs(int pf)
{
	return pf_attr_get_u32(pf, "device/sriov_totalvfs");
}

/**
 * igt_sriov_get_numvfs - Get number of enabled VFs
 * @pf: PF device file descriptor
 *
 * Number of enabled VFs is checked by reading sriov_numvfs attribute
 * corresponding to @pf device.
 *
 * It asserts on failure.
 *
 * Return:
 * Number of VFs enabled by given PF.
 */
unsigned int igt_sriov_get_enabled_vfs(int pf)
{
	return pf_attr_get_u32(pf, "device/sriov_numvfs");
}

/**
 * igt_sriov_enable_vfs - Enable VFs
 * @pf: PF device file descriptor
 * @num_vfs: Number of virtual functions to be enabled
 *
 * Enable VFs by writing @num_vfs to sriov_numvfs attribute corresponding to
 * @pf device.
 * It asserts on failure.
 */
void igt_sriov_enable_vfs(int pf, unsigned int num_vfs)
{
	igt_assert(num_vfs > 0);

	igt_debug("Enabling %u VFs\n", num_vfs);
	pf_attr_set_u32(pf, "device/sriov_numvfs", num_vfs);
}

/**
 * igt_sriov_disable_vfs - Disable VFs
 * @pf: PF device file descriptor
 *
 * Disable VFs by writing 0 to sriov_numvfs attribute corresponding to @pf
 * device.
 * It asserts on failure.
 */
void igt_sriov_disable_vfs(int pf)
{
	pf_attr_set_u32(pf, "device/sriov_numvfs", 0);
}

/**
 * igt_sriov_is_driver_autoprobe_enabled - Get VF driver autoprobe setting
 * @pf: PF device file descriptor
 *
 * Get current VF driver autoprobe setting by reading sriov_drivers_autoprobe
 * attribute corresponding to @pf device.
 *
 * It asserts on failure.
 *
 * Return:
 * True if autoprobe is enabled, false otherwise.
 */
bool igt_sriov_is_driver_autoprobe_enabled(int pf)
{
	return pf_attr_get_u32(pf, "device/sriov_drivers_autoprobe");
}

/**
 * igt_sriov_enable_driver_autoprobe - Enable VF driver autoprobe
 * @pf: PF device file descriptor
 *
 * Enable VF driver autoprobe setting by writing 1 to sriov_drivers_autoprobe
 * attribute corresponding to @pf device.
 *
 * If successful, kernel will automatically bind VFs to a compatible driver
 * immediately after they are enabled.
 * It asserts on failure.
 */
void igt_sriov_enable_driver_autoprobe(int pf)
{
	pf_attr_set_u32(pf,  "device/sriov_drivers_autoprobe", true);
}

/**
 * igt_sriov_disable_driver_autoprobe - Disable VF driver autoprobe
 * @pf: PF device file descriptor
 *
 * Disable VF driver autoprobe setting by writing 0 to sriov_drivers_autoprobe
 * attribute corresponding to @pf device.
 *
 * During VFs enabling driver won't be bound to VFs.
 * It asserts on failure.
 */
void igt_sriov_disable_driver_autoprobe(int pf)
{
	pf_attr_set_u32(pf,  "device/sriov_drivers_autoprobe", false);
}

/**
 * igt_sriov_open_vf_drm_device - Open VF DRM device node
 * @pf: PF device file descriptor
 * @vf_num: VF number (1-based to identify single VF)
 *
 * Open DRM device node for given VF.
 *
 * Return:
 * VF file descriptor or -1 on error.
 */
int igt_sriov_open_vf_drm_device(int pf, unsigned int vf_num)
{
	char dir_path[PATH_MAX], path[256], dev_name[16];
	DIR *dir;
	struct dirent *de;
	bool found = false;

	if (!vf_num)
		return -1;

	if (!igt_sysfs_path(pf, path, sizeof(path)))
		return -1;
	/* vf_num is 1-based, but virtfn is 0-based */
	snprintf(dir_path, sizeof(dir_path), "%s/device/virtfn%u/drm", path, vf_num - 1);

	dir = opendir(dir_path);
	if (!dir)
		return -1;
	while ((de = readdir(dir))) {
		unsigned int card_num;

		if (sscanf(de->d_name, "card%d", &card_num) == 1) {
			snprintf(dev_name, sizeof(dev_name), "/dev/dri/card%u", card_num);
			found = true;
			break;
		}
	}
	closedir(dir);

	if (!found)
		return -1;

	return __drm_open_device(dev_name, DRIVER_ANY);
}

/**
 * igt_sriov_is_vf_drm_driver_probed - Check if VF DRM driver is probed
 * @pf: PF device file descriptor
 * @vf_num: VF number (1-based to identify single VF)
 *
 * Verify if DRM driver is bound to VF device. Probe check is based on
 * existence of the DRM subsystem attribute in sysfs.
 *
 * Returns:
 * True if VF has DRM driver loaded, false if not.
 */
bool igt_sriov_is_vf_drm_driver_probed(int pf, unsigned int vf_num)
{
	char path[PATH_MAX];
	int sysfs;
	bool ret;

	igt_assert(vf_num > 0);

	sysfs = igt_sysfs_open(pf);
	igt_assert_fd(sysfs);
	/* vf_num is 1-based, but virtfn is 0-based */
	snprintf(path, sizeof(path), "device/virtfn%u/drm", vf_num - 1);
	ret = igt_sysfs_has_attr(sysfs, path);
	close(sysfs);

	return ret;
}

static bool __igt_sriov_bind_vf_drm_driver(int pf, unsigned int vf_num, bool bind)
{
	struct pci_device *pci_dev;
	char pci_slot[14];
	int sysfs;
	bool ret;

	igt_assert(vf_num > 0);

	pci_dev = __igt_device_get_pci_device(pf, vf_num);
	igt_assert_f(pci_dev, "No PCI device for given VF number: %d\n", vf_num);
	sprintf(pci_slot, "%04x:%02x:%02x.%x",
		pci_dev->domain_16, pci_dev->bus, pci_dev->dev, pci_dev->func);

	sysfs = igt_sysfs_open(pf);
	igt_assert_fd(sysfs);

	igt_debug("vf_num: %u, pci_slot: %s\n", vf_num, pci_slot);
	ret = igt_sysfs_set(sysfs, bind ? "device/driver/bind" : "device/driver/unbind", pci_slot);

	close(sysfs);

	return ret;
}

/**
 * igt_sriov_bind_vf_drm_driver - Bind DRM driver to VF
 * @pf: PF device file descriptor
 * @vf_num: VF number (1-based to identify single VF)
 *
 * Bind the DRM driver to given VF.
 * It asserts on failure.
 */
void igt_sriov_bind_vf_drm_driver(int pf, unsigned int vf_num)
{
	igt_assert(__igt_sriov_bind_vf_drm_driver(pf, vf_num, true));
}

/**
 * igt_sriov_unbind_vf_drm_driver - Unbind DRM driver from VF
 * @pf: PF device file descriptor
 * @vf_num: VF number (1-based to identify single VF)
 *
 * Unbind the DRM driver from given VF.
 * It asserts on failure.
 */
void igt_sriov_unbind_vf_drm_driver(int pf, unsigned int vf_num)
{
	igt_assert(__igt_sriov_bind_vf_drm_driver(pf, vf_num, false));
}
