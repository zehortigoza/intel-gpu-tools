// SPDX-License-Identifier: MIT
/*
 * Copyright © 2024 Intel Corporation
 */

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "igt_core.h"
#include "intel_chipset.h"
#include "xe/xe_oa.h"
#include "xe/xe_oa_data_reader.h"

#define MAX(a,b) ((a) > (b) ? (a) : (b))
#define MIN(a,b) ((a) > (b) ? (b) : (a))

static void
usage(void)
{
	printf("Usage: xe-perf-reader [options] file\n"
	       "Reads the content of an xe-perf recording.\n"
	       "\n"
	       "     --help,    -h             Print this screen\n"
	       "     --counters, -c c1,c2,...  List of counters to display values for.\n"
	       "                               Use 'all' to display all counters.\n"
	       "                               Use 'list' to list available counters.\n"
	       "     --reports, -r             Print out data per report.\n");
}

static struct intel_xe_perf_logical_counter *
find_counter(struct intel_xe_perf_metric_set *metric_set,
	     const char *name)
{
	for (uint32_t i = 0; i < metric_set->n_counters; i++) {
		if (!strcmp(name, metric_set->counters[i].symbol_name)) {
			return &metric_set->counters[i];
		}
	}

	return NULL;
}

static void
append_counter(struct intel_xe_perf_logical_counter ***counters,
	       int32_t *n_counters,
	       uint32_t *n_allocated_counters,
	       struct intel_xe_perf_logical_counter *counter)
{
	if (*n_counters < *n_allocated_counters) {
		(*counters)[(*n_counters)++] = counter;
		return;
	}

	*n_allocated_counters = MAX(2, *n_allocated_counters * 2);
	*counters = realloc(*counters,
			    sizeof(struct intel_xe_perf_logical_counter *) *
			    (*n_allocated_counters));
	(*counters)[(*n_counters)++] = counter;
}

static struct intel_xe_perf_logical_counter **
get_logical_counters(struct intel_xe_perf_metric_set *metric_set,
		     const char *counter_list,
		     int32_t *out_n_counters)
{
	struct intel_xe_perf_logical_counter **counters = NULL, *counter;
	uint32_t n_allocated_counters = 0;
	const char *current, *next;
	char counter_name[100];

	if (!counter_list) {
		*out_n_counters = 0;
		return NULL;
	}

	if (!strcmp(counter_list, "list")) {
		uint32_t longest_name = 0;

		*out_n_counters = -1;
		for (uint32_t i = 0; i < metric_set->n_counters; i++) {
			longest_name = MAX(longest_name,
					   strlen(metric_set->counters[i].symbol_name));
		}

		fprintf(stdout, "Available counters:\n");
		for (uint32_t i = 0; i < metric_set->n_counters; i++) {
			fprintf(stdout, "%s:%*s%s\n",
				metric_set->counters[i].symbol_name,
				(int)(longest_name -
				      strlen(metric_set->counters[i].symbol_name) + 1), " ",
				metric_set->counters[i].name);
		}
		return NULL;
	}

	if (!strcmp(counter_list, "all")) {
		counters = malloc(sizeof(*counters) * metric_set->n_counters);
		*out_n_counters = metric_set->n_counters;
		for (uint32_t i = 0; i < metric_set->n_counters; i++)
			counters[i] = &metric_set->counters[i];
		return counters;
	}

	*out_n_counters = 0;
	current = counter_list;
	while ((next = strstr(current, ","))) {
		snprintf(counter_name,
			 MIN((uint32_t)(next - current) + 1, sizeof(counter_name)),
			 "%s", current);

		counter = find_counter(metric_set, counter_name);
		if (!counter) {
			fprintf(stderr, "Unknown counter '%s'.\n", counter_name);
			free(counters);
			*out_n_counters = -1;
			return NULL;
		}

		append_counter(&counters, out_n_counters, &n_allocated_counters, counter);

		current = next + 1;
	}

	if (strlen(current) > 0) {
		counter = find_counter(metric_set, current);
		if (!counter) {
			fprintf(stderr, "Unknown counter '%s'.\n", current);
			free(counters);
			*out_n_counters = -1;
			return NULL;
		}

		append_counter(&counters, out_n_counters, &n_allocated_counters, counter);
	}

	return counters;
}

static void
print_report_deltas(const struct intel_xe_perf_data_reader *reader,
		    const struct intel_xe_perf_record_header *xe_report0,
		    const struct intel_xe_perf_record_header *xe_report1,
		    struct intel_xe_perf_logical_counter **counters,
		    uint32_t n_counters)
{
	struct intel_xe_perf_accumulator accu;

	intel_xe_perf_accumulate_reports(&accu,
				      reader->perf, reader->metric_set,
				      xe_report0, xe_report1);

	for (uint32_t c = 0; c < n_counters; c++) {
		struct intel_xe_perf_logical_counter *counter = counters[c];

		switch (counter->storage) {
		case INTEL_XE_PERF_LOGICAL_COUNTER_STORAGE_UINT64:
		case INTEL_XE_PERF_LOGICAL_COUNTER_STORAGE_UINT32:
		case INTEL_XE_PERF_LOGICAL_COUNTER_STORAGE_BOOL32:
			fprintf(stdout, "   %s: %" PRIu64 "\n",
				counter->symbol_name, counter->read_uint64(reader->perf,
									   reader->metric_set,
									   accu.deltas));
			break;
		case INTEL_XE_PERF_LOGICAL_COUNTER_STORAGE_DOUBLE:
		case INTEL_XE_PERF_LOGICAL_COUNTER_STORAGE_FLOAT:
			fprintf(stdout, "   %s: %f\n",
				counter->symbol_name, counter->read_float(reader->perf,
									  reader->metric_set,
									  accu.deltas));
			break;
		}
	}
}

int
main(int argc, char *argv[])
{
	const struct option long_options[] = {
		{"help",             no_argument, 0, 'h'},
		{"counters",   required_argument, 0, 'c'},
		{"reports",          no_argument, 0, 'r'},
		{0, 0, 0, 0}
	};
	struct intel_xe_perf_data_reader reader;
	struct intel_xe_perf_logical_counter **counters;
	const struct intel_device_info *devinfo;
	const char *counter_names = NULL;
	int32_t n_counters;
	int fd, opt;
	bool print_reports = false;

	while ((opt = getopt_long(argc, argv, "hc:r", long_options, NULL)) != -1) {
		switch (opt) {
		case 'h':
			usage();
			return EXIT_SUCCESS;
		case 'c':
			counter_names = optarg;
			break;
		case 'r':
			print_reports = true;
			break;
		default:
			fprintf(stderr, "Internal error: "
				"unexpected getopt value: %d\n", opt);
			usage();
			return EXIT_FAILURE;
		}
	}

	if (optind >= argc) {
		fprintf(stderr, "No recording file specified.\n");
		return EXIT_FAILURE;
	}

	fd = open(argv[optind], 0, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "Cannot open '%s': %s.\n",
			argv[optind], strerror(errno));
		return EXIT_FAILURE;
	}

	if (!intel_xe_perf_data_reader_init(&reader, fd)) {
		fprintf(stderr, "Unable to parse '%s': %s.\n",
			argv[optind], reader.error_msg);
		return EXIT_FAILURE;
	}

	counters = get_logical_counters(reader.metric_set, counter_names, &n_counters);
	if (n_counters < 0)
		goto exit;

	devinfo = intel_get_device_info(reader.devinfo.devid);

	fprintf(stdout, "Recorded on device=0x%x(%s) graphics_ver=%i\n",
		reader.devinfo.devid, devinfo->codename,
		reader.devinfo.graphics_ver);
	fprintf(stdout, "Metric used : %s (%s) uuid=%s\n",
		reader.metric_set->symbol_name, reader.metric_set->name,
		reader.metric_set->hw_config_guid);
	fprintf(stdout, "Reports: %u\n", reader.n_records);
	fprintf(stdout, "Context switches: %u\n", reader.n_timelines);
	fprintf(stdout, "Timestamp correlation points: %u\n", reader.n_correlations);

	if (reader.n_correlations < 2) {
		fprintf(stderr, "Less than 2 CPU/GPU timestamp correlation points.\n");
		return EXIT_FAILURE;
	}

	fprintf(stdout, "Timestamp correlation CPU range:       0x%016"PRIx64"-0x%016"PRIx64"\n",
		reader.correlations[0]->cpu_timestamp,
		reader.correlations[reader.n_correlations - 1]->cpu_timestamp);
	fprintf(stdout, "Timestamp correlation GPU range (64b): 0x%016"PRIx64"-0x%016"PRIx64"\n",
		reader.correlations[0]->gpu_timestamp,
		reader.correlations[reader.n_correlations - 1]->gpu_timestamp);
	fprintf(stdout, "Timestamp correlation GPU range (32b): 0x%016"PRIx64"-0x%016"PRIx64"\n",
		reader.correlations[0]->gpu_timestamp & 0xffffffff,
		reader.correlations[reader.n_correlations - 1]->gpu_timestamp & 0xffffffff);

	fprintf(stdout, "OA data timestamp range:               0x%016"PRIx64"-0x%016"PRIx64"\n",
		intel_xe_perf_read_record_timestamp(reader.perf,
						 reader.metric_set,
						 reader.records[0]),
		intel_xe_perf_read_record_timestamp(reader.perf,
						 reader.metric_set,
						 reader.records[reader.n_records - 1]));
	fprintf(stdout, "OA raw data timestamp range:           0x%016"PRIx64"-0x%016"PRIx64"\n",
		intel_xe_perf_read_record_timestamp_raw(reader.perf,
						     reader.metric_set,
						     reader.records[0]),
		intel_xe_perf_read_record_timestamp_raw(reader.perf,
						     reader.metric_set,
						     reader.records[reader.n_records - 1]));

	if (strcmp(reader.metric_set_uuid, reader.metric_set->hw_config_guid)) {
		fprintf(stdout,
			"WARNING: Recording used a different HW configuration.\n"
			"WARNING: This could lead to inconsistent counter values.\n");
	}

	for (uint32_t i = 0; i < reader.n_timelines; i++) {
		const struct intel_xe_perf_timeline_item *item = &reader.timelines[i];

		fprintf(stdout, "Time: CPU=0x%016" PRIx64 "-0x%016" PRIx64
			" GPU=0x%016" PRIx64 "-0x%016" PRIx64"\n",
			item->cpu_ts_start, item->cpu_ts_end,
			item->ts_start, item->ts_end);
		fprintf(stdout, "hw_id=0x%x %s\n",
			item->hw_id, item->hw_id == 0xffffffff ? "(idle)" : "");

		print_report_deltas(&reader,
				    reader.records[item->record_start],
				    reader.records[item->record_end],
				    counters, n_counters);

		if (print_reports) {
			for (uint32_t r = item->record_start; r < item->record_end; r++) {
				fprintf(stdout, " report%i = %s\n",
					r - item->record_start,
					intel_xe_perf_read_report_reason(reader.perf, reader.records[r]));
				print_report_deltas(&reader,
						    reader.records[r],
						    reader.records[r + 1],
						    counters, n_counters);
			}
		}
	}

 exit:
	intel_xe_perf_data_reader_fini(&reader);
	close(fd);

	return EXIT_SUCCESS;
}
