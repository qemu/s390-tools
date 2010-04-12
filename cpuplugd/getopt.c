/*
 * Copyright IBM Corp 2007
 * Author: Hans-Joachim Picht <hans@linux.vnet.ibm.com>
 *
 * Linux for System z Hotplug Daemon
 *
 * This file is used to parse the command line arguments
 *
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <signal.h>
#include <syslog.h>
#include <limits.h>
#include "cpuplugd.h"

void print_usage(int is_error, char program_name[]);
void print_version();
int foreground;
int debug;
char *configfile;
int cpu_idle_limit;

void parse_options(int argc, char **argv)
{
	int config_file_specified = -1;
	const struct option long_options[] = {
		{ "help", no_argument,       NULL, 'h'},
		{ "foreground", no_argument, NULL, 'f' },
		{ "config", required_argument, NULL, 'c' },
		{ "version", no_argument, NULL, 'v' },
		{ "verbose", no_argument, NULL, 'V'   },
		{ NULL, 0, NULL, 0}
	};

	/* dont run without any argument */
	if (argc == 0 || argc == 1)
		print_usage(0, argv[0]);
	while (optind < argc) {
		int index = -1;
		struct option *opt = 0;
		int result = getopt_long(argc, argv, "hfc:vVm",
			long_options, &index);
		if (result == -1)
			break;		/* end of list */
		switch (result) {
		case 'h':
			print_usage(0, argv[0]);
			break;
		case 'f':
			foreground = 1;
			break;
		case 'c':
			/*
			 * this prevents -cbla and enforces the
			 * user to specify -c bla
			 */
			if (strcmp(argv[optind-1], optarg) == 0) {
				configfile = optarg;
				config_file_specified = 1;
			} else {
				printf("Unrecognized option: %s\n", optarg);
				exit(1);
			}
			break;
		case 'v':
			print_version();
			break;
		case 'V':
			debug = 1;
			break;
		case 0:
			/* all parameter that do not
			   appear in the optstring */
			opt = (struct option *)&(long_options[index]);
			printf("'%s' was specified.",
			       opt->name);
			if (opt->has_arg == required_argument)
				printf("Arg: <%s>", optarg);
			printf("\n");
			break;
		case '?':
			printf("Try '%s' --help' for more information.\n",
				argv[0]);
			exit(1);
			break;
		case -1:
			/* we also run in this case if no argument was s
			 * pecified */
			break;
		default:
			print_usage(0, argv[0]);
		}
	}
	if (config_file_specified == -1) {
		printf("You have to specify a configuration file!\n");
		printf("Try '%s' --help' for more information.\n", argv[0]);
		exit(1);
	}
}
