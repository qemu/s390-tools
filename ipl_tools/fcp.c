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
 * return the current reipl type from /sys/firmware/reipl/reipl_type
 * 0 = fcp, 1 = ccw, 2 = nss, -1 = unknown
 */
int get_reipl_type(char *reipltype)
{
	FILE *filp;
	char path[4096];
	int rc;

	strcpy(path, "/sys/firmware/reipl/reipl_type");
	if (access(path, R_OK) == 0) {
		filp = fopen(path, "r");
		if (!filp) {
			fprintf(stderr,	"%s: Can not open /sys/firmware/"
				"reipl/reipl_type: ", name);
			fprintf(stderr, "%s\n", strerror(errno));
			exit(1);
		}
		rc = fscanf(filp, "%s", reipltype);
		fclose(filp);
		if (rc < 0) {
			fprintf(stderr, "%s: Failed to read "
				"/sys/firmware/reipl/reipl_type:", name);
			fprintf(stderr, "%s\n", strerror(errno));
			exit(1);
		}
		if (strncmp(reipltype, "fcp", strlen("fcp")) == 0)
			return T_FCP;
		else if (strncmp(reipltype, "ccw", strlen("ccw")) == 0)
			return T_CCW;
		else if (strncmp(reipltype, "nss", strlen("nss")) == 0)
			return T_NSS;
	} else {
		fprintf(stderr, "%s: Can not open /sys/firmware/reipl/"
			"reipl_type:", name);
		fprintf(stderr, " %s\n", strerror(errno));
		exit(1);
	}
	return -1;
}

/*
 * check if the specified device number is a valid device number
 * which can be found in the /sys/bus/ccw/drivers/zfcp/
 * structure
 */
int isfcpdev(char *devno)
{
	char path[4096];

	sprintf(path, "/sys/bus/ccw/drivers/zfcp/%s", devno);
	if (chdir(path) != 0)
		return -1;
	return 0;
}

/*
 * check if a device, e.g. /dev/sda1 has a valid entry in /proc/partitions
 * return 0 is true, -1 otherwise
 */
int ispartition(char *devno)
{
	FILE *filp;
	size_t bytes_read;
	char buffer[4096];
	char *match;

	filp = fopen("/proc/partitions", "r");
	if (!filp) {
		fprintf(stderr, "%s: Can not open /proc/partitions: %s\n",
			name, strerror(errno));
		return -1;
	}
	while ((bytes_read = fread(buffer, 1, sizeof(buffer), filp)) != 0) {
		buffer[bytes_read] = '\0';
		match = strstr(buffer, devno);
		if (match != NULL)
			return 0;
	}
	fclose(filp);
	return -1;
}

/*
 * return the wwpn of a device
 */
int get_wwpn(char *device, char *wwpn)
{
	FILE *filp;
	char path[4096];
	char buf[4096];
	int rc;

	sprintf(path, "/sys/block/%s/device/wwpn", device);
	filp = fopen(path, "r");
	if (!filp) {
	       fprintf(stderr, "%s: Can not open %s: %s\n",
		       name, path, strerror(errno));
	       return -1;
	}
	rc = fscanf(filp, "%s", buf);
	if (rc <= 0)
	       return -1;
	strncpy(wwpn, buf, 20);
	fclose(filp);
	return 0;
}


/*
 * return the lun of a device
 */
int get_lun(char *device, char *lun)
{
	FILE *filp;
	char buf[4096];
	char path[4096];
	int rc;

	sprintf(path, "/sys/block/%s/device/fcp_lun", device);
	filp = fopen(path, "r");
	if (!filp) {
		fprintf(stderr, "%s: Can not open %s: %s\n",
			name, path, strerror(errno));
		return -1;
	}
	rc = fscanf(filp, "%s", buf);
	if (rc <= 0)
		return -1;
	strncpy(lun, buf, 20);
	fclose(filp);
	return 0;
}

/*
 * return the device number of a device
 */
int get_fcp_devno(char *device, char *devno)
{
	FILE *filp;
	char buf[4096];
	char path[4096];
	int rc;

	sprintf(path, "/sys/block/%s/device/hba_id", device);
	filp = fopen(path, "r");
	if (!filp) {
		fprintf(stderr, "%s: Can not open %s: %s\n",
			name, path, strerror(errno));
		return -1;
	}
	rc = fscanf(filp, "%s", buf);
	if (rc <= 0)
		return -1;
	strcpy(devno, buf);
	fclose(filp);
	return 0;
}

/*
 * get the device from the partition
 */
int get_fcp_dev(char *partition, char *device)
{
	char *start, *stop;
	size_t len;

	/*
	 * find the text between the two slashes
	 * in /dev/sda1, e.g. sda
	 */
	start = strchr(partition, '/');
	stop  = strrchr(partition, '/');
	len = stop-start;
	if (start == NULL || stop == NULL || len <= 0)
		return -1;
	strncpy(device, stop+1, len-1);
	device[len] = '\0';
	return 0;
}
