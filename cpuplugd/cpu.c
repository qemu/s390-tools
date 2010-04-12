/*
 * Copyright IBM Corp 2007
 * Author: Hans-Joachim Picht <hans@linux.vnet.ibm.com>
 *
 *  Linux for System z Hotplug Daemon
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
#include "cpuplugd.h"

static const int max_cpus = MAX_CPU;

/*
 * return overall number of available cpus. this does not necessarily
 * mean that those are currently online
 */
int get_numcpus()
{
	int i;
	char path[4069];
	int number = 0;

	for (i = 0; i <= max_cpus; i++) {
		/* check whether file exists and is readable */
		sprintf(path, "/sys/devices/system/cpu/cpu%d/online", i);
		if (access(path, R_OK) == 0)
			number++;
	}
	return number;
}

/*
 * return number of online cpus
 */
int get_num_online_cpus()
{
	FILE *filp;
	int i;
	char path[4069];
	int status = 0;
	int value_of_onlinefile, rc;

	for (i = 0; i <= get_numcpus(); i++) {
		/* check wether file exists and is readable */
		sprintf(path, "/sys/devices/system/cpu/cpu%d/online", i);
		if (access(path, R_OK) != 0)
			continue;
		filp = fopen(path, "r");
		if (!filp) {
			fprintf(stderr,	"Can not open cpu online file:");
			fprintf(stderr, "%s\n", strerror(errno));
			if (foreground == 0)
				syslog(LOG_ERR, "Can not open cpu "
				       "online file:%s\n", strerror(errno));
			exit(1);
		} else {
			rc = fscanf(filp, "%d", &value_of_onlinefile);
			if (rc != 1) {
				fprintf(stderr, "Cannot read cpu online "
					"file: %s\n", strerror(errno));
				if (foreground == 0)
					syslog(LOG_ERR, "Cannot read cpu "
					       "online file:%s\n",
					       strerror(errno));
				exit(1);
			}
			if (value_of_onlinefile == 1)
				status++;
		}
		fclose(filp);
	}
	return status;
}

/*
 * enable a certain cpu
 */
int hotplug(int cpuid)
{
	FILE *filp;
	char path[4096];
	int status, rc;

	sprintf(path, "/sys/devices/system/cpu/cpu%d/online", cpuid);
	if (access(path, W_OK) == 0) {
		filp = fopen(path, "w");
		if (!filp) {
			fprintf(stderr,	"Can not open cpu online file:");
			fprintf(stderr, "%s\n", strerror(errno));
			if (foreground == 0)
				syslog(LOG_ERR, "Can not open cpu "
				       "online file:%s\n", strerror(errno));
			exit(1);
		}
		fprintf(filp, "1");
		fclose(filp);
		/*
		 * check if the attempt to enable the cpus really worked
		 */
		filp = fopen(path, "r");
		rc = fscanf(filp, "%d", &status);
		if (rc != 1) {
			fprintf(stderr,	"Can not open cpu online file:");
			fprintf(stderr, "%s\n", strerror(errno));
			if (foreground == 0)
				syslog(LOG_ERR, "Can not open cpu "
				       "online file:%s\n", strerror(errno));
			exit(1);
		}
		fclose(filp);
		if (status == 1) {
			if (debug && foreground == 1)
				printf("cpu with id %d enabled\n", cpuid);
			if (debug && foreground == 0)
				syslog(LOG_INFO, "cpu with id %d enabled\n",
					cpuid);
			return 1;
		} else {
			if (debug && foreground == 1)
				printf("failed to enable cpu with id %d\n",
					cpuid);
			if (debug && foreground == 0)
				syslog(LOG_INFO, "failed to enable cpu with id"
					" %d\n", cpuid);
			return -1;
		}
	} else {
		fprintf(stderr, "hotplugging cpu with id %d failed\n", cpuid);
		if (foreground == 0)
			syslog(LOG_ERR,
			       "hotplugging cpu with id %d failed\n", cpuid);
		return -1;
	}
	return -1;
}

/*
 * disable a certain cpu
 */
int hotunplug(int cpuid)
{
	FILE *filp;
	int state, rc;
	int retval = -1;
	char path[4096];

	state = -1;
	sprintf(path, "/sys/devices/system/cpu/cpu%d/online", cpuid);
	if (access(path, W_OK) == 0) {
		filp = fopen(path, "w");
		fprintf(filp, "0");
		fclose(filp);
		/*
		 * check if the attempt to enable the cpus really worked
		 */
		filp = fopen(path, "r");
		rc = fscanf(filp, "%d", &state);
		if (rc != 1) {
			if (foreground == 1)
				fprintf(stderr, "Failed to disable cpu with "
					"id %d\n", cpuid);
			if (foreground == 0)
				syslog(LOG_INFO, "Failed to disable cpu with id"
					" %d\n", cpuid);
		}
		fclose(filp);
		if (state == 0)
			return 1;
	} else {
		fprintf(stderr, "unplugging cpu with id %d failed\n", cpuid);
		if (foreground == 0)
			syslog(LOG_ERR,
			       "unplugging cpu with id %d failed\n", cpuid);
	}
	return retval;
}

/*
 * check if a certain cpu is currently online
 */
int is_online(int cpuid)
{
	FILE *filp;
	int state;
	int retval, rc;
	char path[4096];

	retval = -1;
	sprintf(path, "/sys/devices/system/cpu/cpu%d/online", cpuid);
	if (access(path, R_OK) == 0) {
		filp = fopen(path, "r");
		rc = fscanf(filp, "%d", &state);
		if (rc == 1) {
			if (state == 1)
				retval = 1;
			if (state == 0)
				retval = 0;
			fclose(filp);
		}
	}
	return retval;
}


/*
 * cleanup method. if the daemon is stopped, we (re) activate all cpus
 */
void reactivate_cpus()
{
	/*
	 * only enable the number of cpus which where
	 * available at daemon startup time
	 */
	int cpuid, nc;

	cpuid = 0;
	/* suppress verbose messages on exit */
	debug = 0;
       /*
	* we check for num_cpu_start != 0 because we might want to
	* clean up, before we queried for the number on cpus  at
	* startup
	*/
	if (num_cpu_start == 0)
		return;
	while (get_num_online_cpus() != num_cpu_start && cpuid < MAX_CPU) {
		nc = get_num_online_cpus();
		if (nc == num_cpu_start)
			return;
		if (nc > num_cpu_start && is_online(cpuid) == 1)
			hotunplug(cpuid);
		if (nc < num_cpu_start && is_online(cpuid) == 0)
			hotplug(cpuid);
		cpuid++;
	}
}

/*
 * in system > 2.6.24 cpus can be deconfigured. the follwoing functions is used
 * to check if a certain cpus is in a deconfigured state.
 */
int cpu_is_configured(int cpuid)
{
	FILE *filp;
	int retval, state, rc;
	char path[4096];

	retval = -1;
	sprintf(path, "/sys/devices/system/cpu/cpu%d/configure", cpuid);
	if (access(path, R_OK) == 0) {
		filp = fopen(path, "r");
		rc = fscanf(filp, "%d", &state);
		if (rc == 1) {
			if (state == 1)
				retval = 1;
			if (state == 0)
				retval = 0;
			fclose(filp);
		}
	}
	return retval;
}
