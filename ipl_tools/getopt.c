/*
 * Copyright IBM Corp 2008
 * Author: Hans-Joachim Picht <hans@linux.vnet.ibm.com>
 *
 * Linux for System z shutdown actions
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

#include <ctype.h>
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
#include "zt_common.h"
#include "chreipl.h"

const char *const usage_ripl =
"Usage: %s TYPE ARGS [OPTIONS]\n"
"\n"
" chreipl ccw -d <DEVICE> [-L <LPARM>]\n"
" chreipl fcp -d <DEVICE> -l <LUN> -w <WWPN> [-b <BPROG>]\n"
" chreipl node <NODE>\n"
" chreipl [-h] [-v]\n"
"\n"
"The types specify the IPL device type:\n"
"  ccw      IPL from CCW device (e.g. DASD)\n"
"  fcp      IPL from FCP device (e.g. SCSI)\n"
"  node     IPL from device specified by device node (e.g. /dev/dasda)\n"
"\n"
"OPTIONS:\n"
"  -d, --device <DEVICE>  Number of the IPL device (ccw) or adapter (fcp)\n"
"  -l  --lun <LUN>        Logical unit number of the IPL device\n"
"  -w  --wwpn <WWPN>      World Wide Port Name of the IPL device\n"
"  -b, --bootprog <BPROG> Bootprog specification\n"
"  -L, --loadparm <PARM> Loadparm specification\n"
"  -h, --help             Print this help, then exit\n"
"  -v, --version          Print version information, then exit\n";

const char *const usage_sa =
"Usage: %s TRIGGER ACTION [COMMAND] [OPTIONS]\n"
"\n"
"Change action to be performed after shutdown.\n"
"\n"
"TRIGGER specifies when the action is performed:\n"
"  halt      System has been shut down (e.g. shutdown -h -H now)\n"
"  poff      System has been shut down for power off (e.g. shutdown -h -P now)\n"
"  reboot    System has been shut down for reboot (e.g. shutdown -r)\n"
"  Note: Depending on the distribution, \"halt\" might be mapped to \"poff\".\n"
"\n"
"ACTION specifies the action to be performed:\n"
"  ipl       IPL with previous settings\n"
"  reipl     IPL with re-IPL settings (see chreipl)\n"
"  stop      Stop all CPUs\n"
"  vmcmd     Perform z/VM command defined by COMMAND\n"
"\n"
"COMMAND defines the z/VM command to issue.\n"
"\n"
"OPTIONS:\n"
"  -h, --help        Print this help, then exit\n"
"  -v, --version     Print version information, then exit\n";


const char *const usage_lsreipl =
"Usage: %s [OPTIONS]\n"
"\n"
"Show re-IPL or IPL settings.\n"
"\n"
"OPTIONS:\n"
"  -i, --ipl		Print the IPL setting\n"
"  -h, --help		Print this help, then exit\n"
"  -v, --version		Print version information, then exit\n";

const char *const usage_lsshut =
"Usage: %s [OPTIONS]\n"
"\n"
"Show actions to be taken after the kernel has shut down.\n"
"\n"
"OPTIONS:\n"
"  -h, --help		Print this help, then exit\n"
"  -v, --version		Print version information, then exit\n";

void print_usage_lsshut(char program_name[])
{
	fprintf(stdout, usage_lsshut, program_name);
	exit(0);
}

void print_usage_lsreipl(char program_name[])
{
	fprintf(stdout, usage_lsreipl, program_name);
	exit(0);
}

void print_usage_sa(char program_name[])
{
	fprintf(stdout, usage_sa, program_name);
	exit(0);
}

void print_usage_ripl(char program_name[])
{
	fprintf(stdout, usage_ripl, program_name);
	exit(0);
}

void print_version()
{
	printf("%s: Linux on System z shutdown actions version %s\n",
		name, RELEASE_STRING);
	printf("Copyright IBM Corp. 2008\n");
	exit(0);
}

void parse_lsshut_options(int argc, char *argv[])
{

	int index, result;
	const struct option long_options[] = {
		{ "help",	 no_argument,		NULL, 'h' },
		{ "version",	 no_argument,		NULL, 'v' },
		{ NULL,		 0,			NULL,  0  }
	};

	while (optind < argc) {
		index = -1;
		struct option *opt = 0;
		result = getopt_long(argc, argv, "hv",
			long_options, &index);
		if (result == -1)
			break;		/* end of list */
		switch (result) {
		case 'h':
			print_usage_lsshut(argv[0]);
			break;
		case 'v':
			print_version();
			break;
		case 0:
			/*
			 * all parameter that do not appear in the optstring
			 */
			opt = (struct option *)&(long_options[index]);
			fprintf(stderr, "%s '%s' was specified.",
			       name, opt->name);
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
			/*
			 * we also run in this case if no argument was specified
			 */
			break;
		default:
			print_usage_lsshut(argv[0]);
			break;
		}
	}
	/* dont run with too many arguments */
	if (argc > 1)
		print_usage_lsshut(argv[0]);
}

static void set_devno(char *optarg)
{
	size_t size;

	size = strlen(optarg);
	if (size == 4) {
		/*
		 * the file below /sys expects the
		 * eight digit notation
		 */
		strncat(devno, "0.0.", 4);
		strncat(devno, optarg, 4);
		strlow(devno);
	} else if (size == 8) {
		strncpy(devno, optarg, sizeof(devno));
		strlow(devno);
		devno[8] = '\0';
	} else {
		fprintf(stderr, "%s: Invalid device number specified\n", name);
		exit(1);
	}
	devno_set = 1;
}

static void set_loadparm(char *optarg)
{
	strncpy(loadparm, optarg, sizeof(loadparm));
	/* is an empty string has been specified */
	if (strncmp(loadparm, " ", strlen(loadparm)) == 0)
		loadparm[0] = '\0';
	if (strlen(loadparm) > 8) {
		fprintf(stderr, "%s: Loadparm may not exceed 8 characters!\n",
			name);
		exit(1);
	}
	loadparm_set = 1;
}

static void set_bootprog(char *optarg)
{
	if (!isdigit(*optarg)) {
		fprintf(stderr, "%s: Only digits allowed for bootprog\n", name);
		exit(1);
	}
	bootprog = atoi(optarg);
	bootprog_set = 1;
}

static void set_lun(char *optarg)
{
	unsigned long long lun_tmp;
	char *endptr;

	lun_tmp = strtoull(optarg, &endptr, 16);
	if (*endptr) {
		fprintf(stderr, "%s: LUN has to be a hex number\n", name);
		exit(1);
	}
	sprintf(lun, "0x%016llx", lun_tmp);
	lun_set = 1;
}

static void set_wwpn(char *optarg)
{
	unsigned long long wwpn_tmp;
	char *endptr;

	wwpn_tmp = strtoull(optarg, &endptr, 16);
	if (*endptr) {
		fprintf(stderr, "%s: WWPN has to be a hex number\n", name);
		exit(1);
	}
	sprintf(wwpn, "0x%016llx", wwpn_tmp);
	wwpn_set = 1;
}

static void check_blank(char *optarg1, char *optarg2)
{
	/*
	 * this prevents e.g. -d4711 and enforces the
	 * user to specify -d 4711
	 */
	if (strcmp(optarg1, optarg2) != 0) {
		fprintf(stderr, "%s: Unrecognized option: %s\n", name, optarg);
		exit(1);
	}
}

static void parse_fcp_args(char *nargv[], int nargc)
{
	/*
	 * we might be called like this:
	 * chreipl fcp 4711 0x12345... 0x12345...
	 */
	if (devno_set || wwpn_set || lun_set) {
		fprintf(stderr, "%s: Use either options or possitional "
			"parameters.\n", name);
		exit(1);
	}
	if (nargc > 3) {
		fprintf(stderr, "%s: Too many arguments specified for "
			"fcp action.\n", name);
		exit(1);
	} else if (nargc != 3) {
		fprintf(stderr, "%s: fcp action requires device, WWPN and "
			"LUN\n", name);
		exit(1);
	}
	set_devno(nargv[0]);
	set_wwpn(nargv[1]);
	set_lun(nargv[2]);
}

static void parse_ccw_args(char *nargv[], int nargc)
{
	/*
	 * we might be called like this:
	 * chreipl ccw 4711
	 */
	if (devno_set) {
		fprintf(stderr, "%s: Use either options or possitional "
			"parameters.\n", name);
		exit(1);
	}
	if (nargc == 0) {
		fprintf(stderr, "%s: ccw action requires device.\n", name);
		exit(1);
	} else if (nargc > 1) {
		fprintf(stderr, "%s: Too many arguments specified for"
			" ccw action.\n", name);
		exit(1);
	}
	set_devno(nargv[0]);
}

static void parse_node_args(char *nargv[], int nargc)
{
	if (nargc != 1) {
		fprintf(stderr, "%s: No device node specified.\n", name);
		exit(1);
	}

	/* a valid entry has to start with /dev/ */
	/* XXX no valid assumption to check only strings... */
	if (strlen(nargv[0]) < 8) {
		fprintf(stderr, "%s: Invalid device node specified\n", name);
		exit(1);
	}
	if  (strncmp(nargv[0], "/dev/", 5) >= 0) {
		/* distinguish between fcp and dasd */
		if (strstr(nargv[0], "dasd") != NULL) {
			use_ccw = 1;
			strncpy(partition, nargv[0], sizeof(partition));
		} else if (strstr(nargv[0], "sd") != NULL) {
			use_ccw = 0;
			strncpy(partition, nargv[0], sizeof(partition));
		} else {
			fprintf(stderr, "%s: Invalid device node specified.\n",
				name);
			exit(1);
		}
	} else {
		fprintf(stderr, "%s: Invalid argument specified.\n", name);
		exit(1);
	}
	partition_set = 1;
}

static void parse_args(char *nargv[], int nargc)
{
	switch (action) {
	case ACT_FCP:
		parse_fcp_args(nargv, nargc);
		break;
	case ACT_CCW:
		parse_ccw_args(nargv, nargc);
		break;
	case ACT_NODE:
		parse_node_args(nargv, nargc);
		break;
	default:
		fprintf(stderr, "%s: Invalid action specification\n", name);
		exit(1);
	}
}

static void check_fcp_opts(void)
{
	if (loadparm_set) {
		fprintf(stderr, "%s: Loadparm only works with dasd\n", name);
		exit(1);
	}
	if (!(devno_set && wwpn_set && lun_set)) {
		fprintf(stderr, "%s: fcp action requires device, WWPN and"
			" LUN\n", name);
		exit(1);
	}
}

static void check_ccw_opts(void)
{
	if (bootprog_set) {
		fprintf(stderr, "%s: bootprog only available with fcp!\n",
			name);
		exit(1);
	}
	if (lun_set || wwpn_set) {
		fprintf(stderr, "%s: LUN or WWPN can not be used with CCW"
			" devices\n", name);
		exit(1);
	}
	if (!devno_set) {
		fprintf(stderr, "%s: ccw action requires device.\n", name);
		exit(1);
	}
}

static void check_node_opts(void)
{
	if (!use_ccw && loadparm_set) {
		fprintf(stderr, "%s: loadparm only works with dasd\n", name);
		exit(1);
	}
	if (use_ccw && bootprog_set) {
		fprintf(stderr, "%s: bootprog only available with fcp!\n",
			name);
		exit(1);
	}
	if (devno_set || wwpn_set || lun_set) {
		fprintf(stderr, "%s: Do not specify devno, WWPN or LUN with "
			"node action.\n", name);
		exit(1);
	}
	if (!partition_set) {
		fprintf(stderr, "%s: node action requires device node.\n",
			name);
		exit(1);
	}
}

void parse_options(int argc, char **argv)
{
	int index, result;

	/*
	 * TOOD: In the future we'll add -P for kernel parameter
	 */
	const struct option long_options[] = {
		{ "help",	 no_argument,		NULL, 'h'},
		{ "bootprog",	 required_argument,	NULL, 'b' },
		{ "device",	 required_argument,	NULL, 'd' },
		{ "lun",	 required_argument,	NULL, 'l' },
		{ "wwpn",	 required_argument,	NULL, 'w' },
		{ "loadparm",	 required_argument,	NULL, 'L' },
		{ "version",	 no_argument,		NULL, 'v' },
		{ NULL,		 0,			NULL,  0  }
	};

	/* dont run without any argument */
	if (argc == 0 || argc == 1)
		print_usage_ripl(argv[0]);
	if  (strncmp(argv[1], "fcp", 3) == 0) {
		action = ACT_FCP;
		use_ccw = 0;
	} else if (strncmp(argv[1], "ccw", 3) == 0) {
		use_ccw = 1;
		action = ACT_CCW;
	} else if (strncmp(argv[1], "node", 4) == 0) {
		action = ACT_NODE;
	}
	/*
	 *TODO: In the future we will have an additional argument called
	 * nss with a parameter -n
	 */
	while (1) {
		index = -1;
		result = getopt_long(argc, argv, "hcd:vw:l:f:L:b:",
				     long_options, &index);
		if (result == -1)
			break;		/* end of list */
		switch (result) {
		case 'h':
			print_usage_ripl(argv[0]);
			break;
		case 'd':
			check_blank(argv[optind - 1], optarg);
			set_devno(optarg);
			break;
		case 'l':
			check_blank(argv[optind - 1], optarg);
			set_lun(optarg);
			break;
		case 'w':
			check_blank(argv[optind - 1], optarg);
			set_wwpn(optarg);
			break;
		case 'L':
			check_blank(argv[optind - 1], optarg);
			set_loadparm(optarg);
			break;
		case 'b':
			check_blank(argv[optind - 1], optarg);
			set_bootprog(optarg);
			break;
		case 'v':
			print_version();
			break;
		case '?':
			printf("Try '%s' --help' for more information.\n",
				name);
			exit(1);
			break;
		case -1:
			/*
			 * we also run in this case if no argument was specified
			 */
			break;
		default:
			print_usage_ripl(argv[0]);
		}
	}
	if (!action) {
		fprintf(stderr, "%s: No valid action specified\n", name);
		exit(1);
	}
	/*
	 * optind is a index which points to the first unrecognised
	 * command line argument
	 */
	if (argc - optind > 1)
		parse_args(&argv[optind + 1], argc - optind - 1);

	/*
	 * Check for valid option combinations
	 */
	switch (action) {
	case ACT_FCP:
		check_fcp_opts();
		break;
	case ACT_CCW:
		check_ccw_opts();
		break;
	case ACT_NODE:
		check_node_opts();
		break;
	}
}

/* action in the chshut mode */
void parse_shutdown_options(int argc, char **argv)
{
	int cse, action, rc, vmcmd_length;
	const struct option long_options[] = {
		{ "help",	 no_argument,		NULL, 'h' },
		{ "version",	 no_argument,		NULL, 'v' },
		{ NULL,		 0,			NULL,  0  }
	};
	char vmcmd[128];

	rc = 0;
	vmcmd_length = 0;
	/* dont run without any argument */
	if (argc == 0 || argc == 1)
		print_usage_sa(argv[0]);
	while (optind < argc) {
		int index = -1;
		struct option *opt = 0;
		int result = getopt_long(argc, argv, "hv",
			long_options, &index);
		if (result == -1)
			break;		/* end of list */
		switch (result) {
		case 'h':
			print_usage_sa(argv[0]);
			break;
		case 'v':
			print_version();
			break;
		case 0:
			/*
			 * all parameter that do not appear in the optstring
			 */
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
			break;
		default:
			print_usage_sa(argv[0]);
		}
	}
	if (argc >= 2) {
		cse = is_valid_case(argv[1]);
		if (cse == -1)  {
			fprintf(stderr, "%s: Invalid case specified\n", name);
			exit(1);
		}
		action = is_valid_action(argv[2]);
		if (action == -1) {
			fprintf(stderr, "%s: Invalid action specified\n", name);
			exit(1);
		}
		/*
		 * vmcmd does not work in a lpar environment and needs an
		 * additional argument
		 */
		if ((action == 4) && (argv[3] != NULL)) {
			if (islpar()  == 0) {
				fprintf(stderr, "%s: vmcmd works only "
					"inside z/VM.\n", name);
				exit(1);
			}
		}
		if ((action == 4) && (argv[3] == NULL)) {
			fprintf(stderr, "%s: vmcmd needs an additional "
				"argument\n", name);
			exit(1);
		}
		/*
		* input needs to be upper case and may not exceed 127 chars
		*/
		if (argv[3] != NULL && strlen(argv[3]) >= 127) {
			fprintf(stderr, "%s: the vmcmd command may not "
				"exceed 127 characters.\n", name);
			exit(1);
		}
		vmcmd_length += strlen(argv[3]);
		switch (cse) {
		case 0:
			rc = strwrt(argv[2],
				"/sys/firmware/shutdown_actions/on_halt");
			if (action == 4 && rc == 0)
				rc = ustrwrt(argv[3],
					"/sys/firmware/vmcmd/on_halt");
			break;
		case 1:
			rc = strwrt(argv[2],
				"/sys/firmware/shutdown_actions/on_poff");
			if (action == 4 && rc == 0)
				rc = ustrwrt(argv[3],
					"/sys/firmware/vmcmd/on_poff");
			break;
		case 2:
			rc = strwrt(argv[2],
				"/sys/firmware/shutdown_actions/on_reboot");
			if (action == 4 && rc == 0) {
				rc = ustrwrt(argv[3],
					"/sys/firmware/vmcmd/on_reboot");
			}
			break;
		}
		if (rc != 0) {
			fprintf(stderr, "%s: Failed to change shutdown"
				" action\n", name);
			exit(1);
		}
		/*
		 * we now allow more than one vmcmd command
		 * example:
		 * chshut reboot vmcmd "MSG..." vmcmd "LOGOFF"
		 */
		int i;
		strncpy(vmcmd, argv[3], strlen(argv[3]));
		for (i = 4; i <= argc; i++) {
			if (argv[i] == NULL)
				exit(1);
			if (strncmp(argv[i], "vmcmd", strlen("vmcmd")) != 0) {
				fprintf(stderr, "%s: Invalid vmcmd command"
					" issued\n", name);
				exit(1);
			}
			if ((strlen(argv[i+1]) + vmcmd_length) >= 127) {
				fprintf(stderr, "%s: the vmcmd command may"
					" not exceed 127 characters.\n", name);
				exit(1);
			}
			strncat(vmcmd, "\n", strlen(argv[i+1]));
			strncat(vmcmd, argv[i+1], strlen(argv[i+1]));
			switch (cse) {
			case 0:
				rc = strwrt(argv[2], "/sys/firmware/"
					"shutdown_actions/on_halt");
				if (action == 4 && rc == 0)
					rc = ustrwrt(vmcmd,
						"/sys/firmware/vmcmd/on_halt");
				break;
			case 1:
				rc = strwrt(argv[2], "/sys/firmware/"
						"shutdown_actions/on_poff");
				if (action == 4 && rc == 0)
					rc = ustrwrt(vmcmd,
						"/sys/firmware/vmcmd/on_poff");
				break;
			case 2:
				rc = strwrt(argv[2],
					"/sys/firmware/shutdown_actions/"
					"on_reboot");
				if (action == 4 && rc == 0) {
					rc = ustrwrt(vmcmd, "/sys/firmware/"
						"vmcmd/on_reboot");
				}
				break;
			}
			vmcmd_length += strlen(argv[i+1]);
			i++;
		}
		if (rc != 0) {
			fprintf(stderr, "%s: Failed to change vmcmd\n", name);
			exit(1);
		}
	}
}


void parse_lsreipl_options(int argc, char **argv)
{
	const struct option long_options[] = {
		{ "help",	 no_argument,		NULL, 'h' },
		{ "ipl",	 no_argument,		NULL, 'i' },
		{ "version",	 no_argument,		NULL, 'v' },
		{ NULL,		 0,			NULL,  0  }
	};

	while (optind < argc) {
		int index = -1;
		struct option *opt = 0;
		int result = getopt_long(argc, argv, "hvi",
			long_options, &index);
		if (result == -1)
			break;		/* end of list */
		switch (result) {
		case 'h':
			print_usage_lsreipl(argv[0]);
			break;
		case 'i':
			print_ipl_settings();
			break;
		case 'v':
			print_version();
			break;
		case 0:
			/*
			 * all parameter that do not appear in the optstring
			 */
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
			/*
			 * we also run in this case if no argument was specified
			 */
			break;
		default:
			print_usage_lsreipl(argv[0]);
			break;
		}
	}
	/* dont run with too many arguments */
	if (argc > 1)
		print_usage_lsreipl(argv[0]);
}
