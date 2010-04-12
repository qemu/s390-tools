/*
 * Copyright IBM Corp 2008
 * Author: Hans-Joachim Picht <hans@linux.vnet.ibm.com>
 *
 *  Linux for System z shutdown actions
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
#include "chreipl.h"


/*
 * return the ipl type based on /sys/firmware/ipl/ipl_type
 * returns 0 in case of fcp and 1 in case of ccw, -1 otherwise
 */
int get_ipl_type()
{
	FILE *filp;
	char reipltype[4];
	char path[4096];
	int rc;

	strcpy(path, "/sys/firmware/ipl/ipl_type");
	if (access(path, R_OK) == 0) {
		filp = fopen(path, "r");
		if (!filp) {
			fprintf(stderr,	"%s: Can not open /sys/firmware/ipl/"
				"ipl_type: ", name);
			fprintf(stderr, "%s\n", strerror(errno));
			return -1;
		}
		rc = fscanf(filp, "%s", reipltype);
		fclose(filp);
		if (rc < 0) {
			fprintf(stderr, "%s: Failed to read "
				"/sys/firmware/ipl/ipl_type: ", name);
			fprintf(stderr, "%s\n", strerror(errno));
			return -1;
		}
		if (strncmp(reipltype, "fcp", strlen("fcp")) == 0)
			return T_FCP;
		else if (strncmp(reipltype, "ccw", strlen("ccw")) == 0)
			return T_CCW;
	} else {
		fprintf(stderr, "%s: Can not open /sys/firmware/ipl/"
			"ipl_type:", name);
		fprintf(stderr, " %s\n", strerror(errno));
	}
	return -1;
}

/*
 * return the loadparameter from /sys/firmware/ipl/loadparm
 */
int get_ipl_loadparam(void)
{
	FILE *filp;
	char path[4096];
	int rc, val;

	strcpy(path, "/sys/firmware/ipl/loadparm");
	if (access(path, R_OK) == 0) {
		filp = fopen(path, "r");
		if (!filp) {
			fprintf(stderr,	"%s: Can not open /sys/firmware/ipl"
				"/loadparm: ", name);
			fprintf(stderr, "%s\n", strerror(errno));
			return -1;
		}
		rc = fscanf(filp, "%d", &val);
		fclose(filp);
		if (rc < 0)
			return -1;
		 else
			return val;
	} else {
		fprintf(stderr, "%s: Can not open /sys/firmware/ipl/"
			"loadparm: ", name);
		fprintf(stderr, "%s\n", strerror(errno));
	}
	return -1;
}

/*
 * print out the settings discovery in the /sys/firmware/ipl filesystem
 * structure
 */

void print_ipl_settings(void)
{
	int rc, type;
	char bootprog[1024], lba[1024];

	type = get_ipl_type();
	/*
	 * TODO: add nss support
	 */
	if (type == 1)
		printf("IPL type:      ccw\n");
	if (type == 0)
		printf("IPL type:      fcp\n");
	rc = strrd(devno, "/sys/firmware/ipl/device");
	if (rc != 0)
		exit(1) /* the error msg comes from get_ipl_device */;
	if (strlen(devno) > 0)
		printf("Device:        %s\n", devno);
	if (type == 1) {
		rc = get_ipl_loadparam();
		if (rc != -1)
			printf("Loadparm:      %d\n", rc);
		else
			printf("Loadparm:      \n");
	}
	if (type == 0) {
		/* these settings are only available for fcp */
		rc = strrd(wwpn, "/sys/firmware/reipl/fcp/wwpn");
		if (rc != -1 && strlen(wwpn) > 0)
			printf("WWPN:          %s\n", wwpn);
		rc = strrd(lun, "/sys/firmware/ipl/lun");
		if (rc != -1 && strlen(lun) > 0)
			printf("LUN:           %s\n", lun);
		rc = strrd(bootprog, "/sys/firmware/reipl/fcp/bootprog");
		if (rc != -1 && strlen(bootprog) > 0)
			printf("bootprog:      %s\n", bootprog);
		rc = strrd(lba, "/sys/firmware/ipl/br_lba");
		if (rc != -1 &&  strlen(lba) > 0)
			printf("br_lba:        %s\n", lba);
	}
	exit(0);
}
