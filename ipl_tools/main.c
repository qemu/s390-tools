/*
 * Copyright IBM Corp 2008
 * Author: Hans-Joachim Picht <hans@linux.vnet.ibm.com>
 *
 * Linux for System z shutdown action
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
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <syslog.h>
#include <pthread.h>
#include <setjmp.h>
#include "chreipl.h"

int use_ccw = 1;	/* default is a ccw device */
char loadparm[9];	/* the entry in the boot menu */
int loadparm_set;
int verbose;		/* default: don't be verbose */
int bootprog;		/* default bootprog value is 0 */
int bootprog_set;
char wwpn[20];		/* 18 character +0x" */
int wwpn_set;
char lun[20];		/* 18 character +0x" */
int lun_set;
char devno[9];		/* device number e.g. 0.0.4711 */
int devno_set;
char device[9];		/* the device itself, e.g. sda1 */
char partition[15];	/* partition, e.g. /dev/dasda1 */
int partition_set;
char devparent[9];	/* the device a partion belongs to. e.g. dasda */
char saction[8];	/* the shutdown action */
char name[256];		/* program name */
int action;		/* either CCW, FCP or NODE */

/*
 * "main" function for the lsreipl related stuff
 */
int lsreipl(int argc, char *argv[])
{
	int rc;
	char bootprog[1024], lba[1024], val[9];

	/* parse the command line options in getop.c */
	parse_lsreipl_options(argc, argv);

	rc = get_reipl_type();
	if (rc == 0) {
		printf("Re-IPL type: fcp\n");
		rc = strrd(wwpn, "/sys/firmware/reipl/fcp/wwpn");
		if (rc != 0)
			exit(1);
		rc = strrd(lun, "/sys/firmware/reipl/fcp/lun");
		if (rc != 0)
			exit(1);
		rc = strrd(devno, "/sys/firmware/reipl/fcp/device");
		if (rc != 0)
			exit(1);
		rc = strrd(bootprog, "/sys/firmware/reipl/fcp/bootprog");
		if (rc != 0)
			exit(1);
		rc = strrd(lba, "/sys/firmware/reipl/fcp/br_lba");
		if (rc != 0)
			exit(1);
		if (strlen(wwpn) > 0)
			printf("WWPN:        %s\n", wwpn);
		if (strlen(lun) > 0)
			printf("LUN:         %s\n", lun);
		if (strlen(devno) > 0)
			printf("Device:      %s\n", devno);
		if (strlen(bootprog) > 0)
			printf("bootprog:    %s\n", bootprog);
		if (strlen(lba) > 0)
			printf("br_lba:      %s\n", lba);
	}
	if (rc == 1) {
		printf("Re-IPL type:   ccw\n");
		rc = strrd(devno, "/sys/firmware/reipl/ccw/device");
		if (rc != 0)
			exit(1);
		if (strlen(devno) > 0)
			printf("Device:        %s\n", devno);
		/*
		 * check if we can read the load parameter (Loadparm)
		 */
		rc = strrd(val, "/sys/firmware/reipl/ccw/loadparm");
		if (rc != -1)
			printf("Loadparm:      %s\n", val);
		else
			printf("Loadparm:      \n");
	}
	return 0;
}

/*
 * "main" function for the reipl related stuff
 *
 */
int reipl(int argc, char *argv[])
{

	int rc, lpval;
	char path[4096];

	lpval = 0;
	/* parse the command line options in getop.c */
	parse_options(argc, argv);
	/*
	 * in case we want to reipl from a ccw device
	 */
	if (use_ccw == 1) {
		if (action == ACT_NODE) {
			rc = get_ccw_dev(partition, device);
			if (rc != 0) {
				fprintf(stderr, "%s: Cannot find device for "
					"partition: %s\n", name, partition);
				exit(1);
			}
			rc = get_ccw_devno(device, devno);
			if (rc != 0)  {
				fprintf(stderr, "%s: Unable to lookup device"
					" number for device %s\n", name,
					device);
				exit(1);
			}
		}
		if (isccwdev(devno) != 0) {
			fprintf(stderr, "%s: Unable to find a valid ccw"
				" device number\n", name);
			exit(1);
		}
		rc = strwrt(devno, "/sys/firmware/reipl/ccw/device");
		if (rc != 0)
			fprintf(stderr, "%s: Failed to set ccw device "
				"number\n", name);
		rc = strwrt("ccw", "/sys/firmware/reipl/reipl_type");
		if (rc != 0)
			fprintf(stderr, "%s: Failed to set reipl type "
				"to ccw\n", name);
		printf("Settings changed to: %s\n", devno);
		printf("Re-IPL type:         ccw\n");
		if (strlen(loadparm) != 0) {
			rc = strwrt(loadparm,
				"/sys/firmware/reipl/ccw/loadparm");
		} else {
			rc = ustrwrt("\n",
				"/sys/firmware/reipl/ccw/loadparm");
		}
		if (rc != 0)
			fprintf(stderr, "%s: Failed to set "
				"loadparm\n", name);
		printf("Loadparm:            %s\n", loadparm);
	}
	/*
	 * in case we reipl from a scsi device
	 */
	if (use_ccw == 0) {
		/*
		 * detect the necessary settings based on the device file:
		 * device number, lun and wwpn
		 */
		if (action == ACT_NODE) {
			/* get the device from the partition */
			rc = get_fcp_dev(partition, device);
			if (rc != 0) {
				fprintf(stderr, "%s Cannot find device for"
					" partition: %s\n", name, partition);
				exit(1);
			}
			if (ispartition(device) != 0) {
				fprintf(stderr, "%s: %s is not a valid "
					"partition\n", name, partition);
				exit(1);
			}
			if (verbose)
				printf("device: found %s\n", device);
			sprintf(path, "/sys/block/%s/device", device);
			if (verbose)
				printf("path is %s\n", path);
			if (chdir(path) != 0) {
				fprintf(stderr, "%s: Cannot find required"
					" data related to device %s\n",
					name, partition);
				exit(1);
			}
			rc = get_wwpn(device, wwpn);
			if (rc != 0) {
				fprintf(stderr, "%s: Failed to lookup "
					"WWPN\n", name);
				exit(1);
			}
			rc = get_lun(device, lun);
			if (rc != 0) {
				fprintf(stderr, "%s: Failed to lookup "
					"LUN\n", name);
				exit(1);
			}
			rc = get_fcp_devno(device, devno);
			if (rc != 0) {
				fprintf(stderr, "%s: Cannot find device for "
					"partition: %s\n", name, partition);
				exit(1);
			}
			if (verbose)
				printf("Device: %s\n", devno);
		}
		if (isfcpdev(devno) != 0) {
			fprintf(stderr, "%s: %s is not a valid fcp device"
				" number.\n", name, devno);
			exit(1);
		}
		rc = strwrt(devno, "/sys/firmware/reipl/fcp/device");
		if (rc != 0) {
			fprintf(stderr, "%s: Failed to set fcp device "
				"number\n", name);
			exit(1);
		}
		rc = strwrt(wwpn, "/sys/firmware/reipl/fcp/wwpn");
		if (rc != 0) {
			fprintf(stderr, "%s: Failed to set WWPN\n", name);
			exit(1);
		}
		rc = strwrt(lun, "/sys/firmware/reipl/fcp/lun");
		if (rc != 0) {
			fprintf(stderr, "%s: Failed to set fcp LUN\n", name);
			exit(1);
		}
		rc = strwrt("fcp", "/sys/firmware/reipl/reipl_type");
		if (rc != 0) {
			fprintf(stderr, "%s: Failed to set reipl type to"
				" fcp\n", name);
			exit(1);
		}
		/*
		 * set the boot record logical block address. Master boot
		 * record. It is always 0 for Linux
		 */
		rc = intwrt(0, "/sys/firmware/reipl/fcp/br_lba");
		if (rc != 0) {
			fprintf(stderr, "%s: Failed to set boot record "
				"logical address to 0\n", name);
			exit(1);
		}
		rc = intwrt(bootprog,  "/sys/firmware/reipl/fcp/bootprog");
		if (rc != 0) {
			fprintf(stderr, "%s: Failed to set "
				"bootprog\n", name);
			exit(1);
		}
		printf("Settings changed to: %s\n", devno);
		printf("WWPN:                %s\n", wwpn);
		printf("LUN:                 %s\n", lun);
		if (bootprog != 0)
			printf("Bootprog:            %d\n", bootprog);
		printf("Re-IPL type:         fcp\n");
	}
	return 0;
}
/* "main" function for list shutdown actions */
int lsshut(int argc, char *argv[])
{
	int rc;
	char tmp[1024];
	char cmd[1024];

	parse_lsshut_options(argc, argv);
	printf("Trigger          Action\n");
	printf("========================\n");
	if (access("/sys/firmware/shutdown_actions/on_halt", R_OK) == 0) {
		rc = get_sa(tmp, "on_halt");
		if (rc != -1) {
			if (strncmp(tmp, "vmcmd", strlen("vmcmd")) == 0) {
				rc = strrd(cmd, "/sys/firmware/vmcmd/on_halt");
				if (rc != 0)
					exit(1);
				printf("Halt             %s (\"%s\")\n", tmp,
						cmd);
			} else
				printf("Halt             %s\n", tmp);
		}

	}
	if (access("/sys/firmware/shutdown_actions/on_panic", R_OK) == 0) {
		rc = get_sa(tmp, "on_panic");
		if (rc != -1) {
			if (strncmp(tmp, "vmcmd", strlen("vmcmd")) == 0) {
				rc = strrd(cmd, "/sys/firmware/vmcmd/on_panic");
				if (rc != 0)
					exit(1);
				printf("Panic            %s (\"%s\")\n", tmp,
						cmd);
			} else
				printf("Panic            %s\n", tmp);
		}
	}
	if (access("/sys/firmware/shutdown_actions/on_poff", R_OK) == 0) {
		rc = get_sa(tmp, "on_poff");
		if (rc != -1) {
			if (strncmp(tmp, "vmcmd", strlen("vmcmd")) == 0) {
				rc = strrd(cmd, "/sys/firmware/vmcmd/on_poff");
				if (rc != 0)
					exit(1);
				printf("Power off        %s (\"%s\")\n", tmp,
						cmd);
			} else
				printf("Power off        %s\n", tmp);
		}
	}
	if (access("/sys/firmware/shutdown_actions/on_reboot", R_OK) == 0) {
		rc = get_sa(tmp, "on_reboot");
		if (rc != -1) {
			if (strncmp(tmp, "vmcmd", strlen("vmcmd")) == 0) {
				rc = strrd(cmd, "/sys/firmware/vmcmd/"
					"on_reboot");
				if (rc != 0)
					exit(1);
				printf("Reboot           %s (\"%s\")\n", tmp,
						cmd);
			} else
				printf("Reboot           %s\n", tmp);
		}
	}
	return 0;
}

/* "main" function for shutdown actions */
int chshut(int argc, char *argv[])
{
	parse_shutdown_options(argc, argv);
	return 0;
}

int main(int argc, char *argv[])
{

	strcpy(name, argv[0]);
	if (check_for_root() != 0) {
		fprintf(stderr, "%s: You must be root to perform this "
			"operation\n", name);
		exit(1);
	}
	/* lets see how we are called */
	if (strstr(argv[0], "chreipl") != NULL) {
		reipl(argc, argv);
		return 0;
	}
	if (strstr(argv[0], "chshut") != NULL) {
		chshut(argc, argv);
		return 0;
	}
	if (strstr(argv[0], "lsreipl") != NULL) {
		lsreipl(argc, argv);
		return 0;
	}
	if (strstr(argv[0], "lsshut") != NULL) {
		lsshut(argc, argv);
		return 0;
	}
	fprintf(stderr, "%s: Dont know how we are called.\n", name);
	return 1;
}
