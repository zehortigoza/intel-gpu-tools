// SPDX-License-Identifier: MIT
/*
 * Copyright © 2022 Intel Corporation
 */

#ifndef IGT_DRM_CLIENTS_H
#define IGT_DRM_CLIENTS_H

#include <stdint.h>

#include "lib/igt_drm_fdinfo.h"

/**
 * SECTION:igt_drm_clients
 * @short_description: Parsing driver exposed fdinfo to track DRM clients
 * @title: DRM clients parsing library
 * @include: igt_drm_clients.h
 *
 * Some DRM drivers expose GPU usage statistics in DRM file descriptor fdinfo
 * data as exposed in /proc. (As documented in kernel's
 * Documentation/gpu/drm-usage-stats.rst.)
 *
 * This library enumerates all DRM clients by parsing that data and tracks them
 * in a list of clients (struct igt_drm_clients) available for inspection
 * after one or more calls to igt_drm_clients_scan.
 */

struct drm_client_fdinfo;

enum igt_drm_client_status {
	IGT_DRM_CLIENT_FREE = 0, /* mbz */
	IGT_DRM_CLIENT_ALIVE,
	IGT_DRM_CLIENT_PROBE
};

enum igt_drm_client_utilization_type {
	IGT_DRM_CLIENT_UTILIZATION_ENGINE_TIME	= 1U << 0,
	IGT_DRM_CLIENT_UTILIZATION_CYCLES	= 1U << 1,
};

struct igt_drm_client_engines {
	unsigned int num_engines; /* Number of discovered active engines. */
	unsigned int max_engine_id; /* Largest engine index discovered.
				       (Can differ from num_engines - 1 when using the engine map facility.) */
	unsigned int *capacity; /* Array of engine capacities as parsed from fdinfo. */
	char **names; /* Array of engine names, either auto-detected or from the passed in engine map. */
};

struct igt_drm_client_regions {
	unsigned int num_regions; /* Number of discovered memory_regions. */
	unsigned int max_region_id; /* Largest memory region index discovered.
				       (Can differ from num_regions - 1 when using the region map facility.) */
	char **names; /* Array of region names, either auto-detected or from the passed in region map. */
};

struct igt_drm_clients;

struct igt_drm_client {
	struct igt_drm_clients *clients; /* Owning list. */

	enum igt_drm_client_status status;
	struct igt_drm_client_regions *regions; /* Memory regions present in this client, to map with memory usage. */
	struct igt_drm_client_engines *engines; /* Engines used by this client, to map with busynees data. */
	unsigned long id; /* DRM client id from fdinfo. */
	unsigned int drm_minor; /* DRM minor of this client. */
	unsigned int pid; /* PID which has this DRM fd open. */
	char pid_str[10]; /* Cached PID representation. */
	char name[24]; /* Process name of the owning PID. */
	char print_name[24]; /* Name without any non-printable characters. */
	unsigned int samples; /* Count of times scanning updated this client. */

	uint32_t utilization_mask; /* mask of enum igt_drm_client_utilization_type */
	unsigned long total_engine_time; /* Aggregate of @utilization.agg_delta_engine_time, i.e. engine time on all engines since client start. */
	unsigned long agg_delta_engine_time; /* Aggregate of @utilization.delta_engine_time, i.e. engine time on all engines since previous scan. */
	unsigned long total_cycles; /* Aggregate of @utilization.agg_delta_cycles, i.e. engine time on all engines since client start. */
	unsigned long agg_delta_cycles; /* Aggregate of @utilization.delta_cycles, i.e. engine time on all engines since previous scan. */
	struct igt_drm_client_utilization {
		unsigned long delta_engine_time; /* Engine time data, relative to previous scan. */
		unsigned long delta_cycles; /* Engine cycles data, relative to previous scan. */
		uint64_t last_engine_time; /* Engine time data as parsed from fdinfo. */
		uint64_t last_cycles; /* Engine cycles data as parsed from fdinfo. */
	} *utilization; /* Array of engine utilization */

	struct drm_client_meminfo *memory; /* Array of region memory utilisation as parsed from fdinfo. */
};

struct igt_drm_clients {
	unsigned int num_clients;
	unsigned int active_clients;

	int max_pid_len;
	int max_name_len;

	void *private_data;

	struct igt_drm_client *client; /* Must be last. */
};

#define igt_for_each_drm_client(clients, c, tmp) \
	for ((tmp) = (clients)->num_clients, c = (clients)->client; \
	     (tmp > 0); (tmp)--, (c)++)

struct igt_drm_clients *igt_drm_clients_init(void *private_data);
void igt_drm_clients_free(struct igt_drm_clients *clients);

struct igt_drm_clients *
igt_drm_clients_scan(struct igt_drm_clients *clients,
		     bool (*filter_client)(const struct igt_drm_clients *,
					   const struct drm_client_fdinfo *),
		     const char **name_map, unsigned int map_entries,
		     const char **region_map, unsigned int region_entries);

struct igt_drm_clients *
igt_drm_clients_sort(struct igt_drm_clients *clients,
		     int (*cmp)(const void *, const void *, void *));

#endif /* IGT_DRM_CLIENTS_H */
