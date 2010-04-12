/*
 * Copyright IBM Corp 2007
 * Author: Hans-Joachim Picht <hans@linux.vnet.ibm.com>
 *
 * Linux for System z Hotplug Daemon
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

/*
 * return the number of runable processes from /proc/loadavg
 */
int get_runable_proc()
{
	FILE *filp;
	float load, load5, load15;
	int runable, rc;

	runable = -1;
	filp = fopen("/proc/loadavg", "r");
	if (!filp) {
		fprintf(stderr, "cannot open kernel loadaverage "
			"statistics: %s\n", strerror(errno));
		if (foreground == 0)
			syslog(LOG_ERR, "cannot open kernel loadaverage "
			       "statistics: %s\n", strerror(errno));
		clean_up();
	}
	rc = fscanf(filp, "%f%f%f%i", &load, &load5, &load15, &runable);
	if (rc != 4) {
		fprintf(stderr, "cannot open kernel loadaverage "
			"statistics: %s\n", strerror(errno));
		if (foreground == 0)
			syslog(LOG_ERR, "cannot open kernel loadaverage "
			       "statistics: %s\n", strerror(errno));
		clean_up();
	}
	fclose(filp);
	return runable;
}

/*
 * return current load average based on /proc/loadavg
 *
 * Example: 0.20 0.18 0.12 1/80 11206
 *
 * The first three columns measure CPU utilization of the last 1, 5,
 * and 15 minute periods.
 * The fourth column shows the number of currently running processes
 * and the total number of processes.
 * The last column displays the last process ID used.
 *
 */
float get_loadavg(void)
{
	FILE *filp;
	float loadavg;
	int rc;

	filp = fopen("/proc/loadavg", "r");
	if (!filp) {
		fprintf(stderr, "cannot open kernel loadaverage "
			"statistics: %s\n", strerror(errno));
		if (foreground == 0)
			syslog(LOG_ERR, "cannot open kernel loadaverage "
			       "statistics: %s\n", strerror(errno));
		clean_up();
	}
	rc = fscanf(filp, "%f", &loadavg);
	if (rc != 1) {
		fprintf(stderr, "cannot open kernel loadaverage "
			"statistics: %s\n", strerror(errno));
		if (foreground == 0)
			syslog(LOG_ERR, "cannot open kernel loadaverage "
			       "statistics: %s\n", strerror(errno));
		clean_up();
	}
	fclose(filp);
	return loadavg;
}

/*
 * return the number of idle ticks from /proc/stat
 */
long long get_idle_ticks()
{
	FILE *filp;
	long long dummy, idle;
	int rc;

	idle = -1;
	filp = fopen("/proc/stat", "r");
	if (!filp) {
		fprintf(stderr, "cannot open kernel cpu "
			"statistics: %s\n", strerror(errno));
		if (foreground == 0)
			syslog(LOG_ERR, "cannot open kernel cpu "
				"statistics: %s\n", strerror(errno));
		clean_up();
	}
	rc = fscanf(filp, "cpu %lld %lld %lld %lld",
		&dummy, &dummy, &dummy, &idle);
	if (rc != 4) {
		fprintf(stderr, "cannot read kernel cpu "
			"statistics: %s\n", strerror(errno));
		if (foreground == 0)
			syslog(LOG_ERR, "cannot open kernel cpu "
				"statistics: %s\n", strerror(errno));
		clean_up();
	}
	fclose(filp);
	return idle;
}
