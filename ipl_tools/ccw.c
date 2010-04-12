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
#include <sys/wait.h>
#include <sys/types.h>
#include <signal.h>
#include <syslog.h>
#include <pthread.h>
#include "chreipl.h"


/*
 * check if the specified device number is a valid device number
 * which can be found in the /sys/bus/ccw/drivers/dasd-eckd/
 * structure
 *
 * this does not work when booting from tape
 */
int isccwdev(const char *devno)
{
	char path[4096];

	sprintf(path, "/sys/bus/ccw/drivers/dasd-eckd/%s", devno);
	if (access(path, R_OK) == 0)
		return 0;
	sprintf(path, "/sys/bus/ccw/drivers/dasd-fba/%s", devno);
	if (access(path, R_OK) == 0)
		return 0;
	return -1;
}


int get_ccw_devno_old_sysfs(char *device, char *devno)
{
	FILE *filp;
	int len, errorpath, rc;
	char path1[4096];
	char buf[4096];
	char *match, *s1, *s2;

	errorpath = 1;
	rc = 0;
	sprintf(path1, "/sys/block/%s/uevent", device);
	filp = fopen(path1, "r");
	if (!filp) {
		rc = -1;
		return rc;
	}
	/*
	 * the uevent file contains an entry like this:
	 * PHYSDEVPATH=/devices/css0/0.0.206a/0.0.7e78
	 */
	while (fscanf(filp, "%s", buf) >= 0) {
		match = strstr(buf, "PHYSDEVPATH");
		if (match != NULL)
			break;
	}
	s1 =  strchr(buf, '/');
	s2 =  strrchr(buf, '/');
	len = s2-s1;
	strncpy(devno, s2 + 1, sizeof(devno));
	devno[len] = '\0';
	fclose(filp);
	return 0;
}

int get_ccw_devno_new_sysfs(char *device, char *devno)
{
	int len, errorpath, rc;
	char path2[4096];
	char buf[4096];
	char *s1, *s2;
	ssize_t linksize;

	errorpath = 1;
	s1 = NULL;
	s2 = NULL;
	len = 0;
	sprintf(path2, "/sys/block/%s/device", device);
	linksize = readlink(path2, buf, sizeof(buf)-1);
	if (linksize == -1) {
		errorpath = 2;
		rc = -1;
		return rc;
	} else {
		/*
		 * the output has the following format:
		 *  ../../../0.0.4e13
		 */
		s1 = strchr(buf, '/');
		s2 = strrchr(buf, '/');
		len = s2 - s1 + 2;
		if (s1 == NULL || s2 == NULL || len <= 0) {
			rc = -1;
			fprintf(stderr, "%s: Can not open %s: %s\n",
					 name, path2, strerror(errno));
			return rc;
		}
	}
	strncpy(devno, s2 + 1, sizeof(devno));
	devno[len] = '\0';
	return 0;
}

/*
 * return the device number for a device
 * dasda can be found in /sys/block/dasda/uevent or in a
 * symbolic link in the same directory. the first file only
 * contains the relevant information if we run on a kernel with
 * has the following kernel option enabled:
 * CONFIG_SYSFS_DEPRECATED
 *
 * This does not work when booting from tape
 */
int get_ccw_devno(char *device, char *devno)
{
    if (get_ccw_devno_old_sysfs(device, devno) != 0) {
	if (get_ccw_devno_new_sysfs(device, devno) != 0) {
		fprintf(stderr, "%s: Failed to lookup the device number\n",
			name);
		return -1;
	}
    }
    return 0;
}

int get_ccw_dev(char *partition, char *device)
{
	char   *stop;

	stop  = strrchr(partition, '/');
	if (stop == NULL)
		 return -1;
	strncpy(device, stop+1, 8);
	return 0;
}
