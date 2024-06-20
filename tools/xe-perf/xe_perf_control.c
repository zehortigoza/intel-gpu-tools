// SPDX-License-Identifier: MIT
/*
 * Copyright © 2024 Intel Corporation
 */


#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "xe_perf_recorder_commands.h"

static void
usage(const char *name)
{
	fprintf(stdout,
		"Usage: %s [options]\n"
		"\n"
		"     --help,               -h         Print this screen\n"
		"     --command-fifo,       -f <path>  Path to a command fifo\n"
		"     --dump,               -d <path>  Write a content of circular buffer to path\n",
		name);
}

int
main(int argc, char *argv[])
{
	const struct option long_options[] = {
		{"help",                       no_argument, 0, 'h'},
		{"dump",                 required_argument, 0, 'd'},
		{"command-fifo",         required_argument, 0, 'f'},
		{"quit",                       no_argument, 0, 'q'},
		{0, 0, 0, 0}
	};
	const char *command_fifo = XE_PERF_RECORD_FIFO_PATH, *dump_file = NULL;
	FILE *command_fifo_file;
	int opt;
	bool quit = false;

	while ((opt = getopt_long(argc, argv, "hd:f:q", long_options, NULL)) != -1) {
		switch (opt) {
		case 'h':
			usage(argv[0]);
			return EXIT_SUCCESS;
		case 'd':
			dump_file = optarg;
			break;
		case 'f':
			command_fifo = optarg;
			break;
		case 'q':
			quit = true;
			break;
		default:
			fprintf(stderr, "Internal error: "
				"unexpected getopt value: %d\n", opt);
			usage(argv[0]);
			return EXIT_FAILURE;
		}
	}

	if (!command_fifo)
		return EXIT_FAILURE;

	command_fifo_file = fopen(command_fifo, "r+");
	if (!command_fifo_file) {
		fprintf(stderr, "Unable to open command file\n");
		return EXIT_FAILURE;
	}

	if (dump_file) {
		if (dump_file[0] == '/') {
			uint32_t total_len =
				sizeof(struct recorder_command_base) + strlen(dump_file) + 1;
			struct {
				struct recorder_command_base base;
				uint8_t dump[];
			} *data = malloc(total_len);

			data->base.command = RECORDER_COMMAND_DUMP;
			data->base.size = total_len;
			snprintf((char *) data->dump, strlen(dump_file) + 1, "%s", dump_file);

			fwrite(data, total_len, 1, command_fifo_file);
		} else {
			char *cwd = getcwd(NULL, 0);
			uint32_t path_len = strlen(cwd) + 1 + strlen(dump_file) + 1;
			uint32_t total_len = sizeof(struct recorder_command_base) + path_len;
			struct {
				struct recorder_command_base base;
				uint8_t dump[];
			} *data = malloc(total_len);

			data->base.command = RECORDER_COMMAND_DUMP;
			data->base.size = total_len;
			snprintf((char *) data->dump, path_len, "%s/%s", cwd, dump_file);

			fwrite(data, total_len, 1, command_fifo_file);
		}
	}

	if (quit) {
		struct recorder_command_base base = {
			.command = RECORDER_COMMAND_QUIT,
			.size = sizeof(base),
		};

		fwrite(&base, sizeof(base), 1, command_fifo_file);
	}

	fclose(command_fifo_file);

	return EXIT_SUCCESS;
}
