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
#include "zt_common.h"
#include "cpuplugd.h"


const char *name = NAME;
static const char *pid_file = PIDFILE;

const char *const usage =
    "Usage: %s [OPTIONS]\n"
    "\n"
    "Daemon to dynamically hotplug cpus and memory based on a set of rules\n"
    "Use OPTIONS described below.\n"
    "\n"
    "\t-c, --config  CONFIGFILE	Path to the configuration file\n"
    "\t-f, --foreground		Run in foreground, do not detach\n"
    "\t-h, --help			Print this help, then exit\n"
    "\t-v, --version			Print version information, then exit\n"
    "\t-V, --verbose			Provide more verbose output\n";

/*
 *  print command usage
 */
void print_usage(int is_error, char program_name[])
{
	fprintf(is_error ? stderr : stdout, usage, program_name);
	exit(is_error ? 1 : 0);
}

/*
 * print command version
 */
void print_version()
{
	printf("%s: Linux on System z CPU hotplug daemon version %s\n",
		name, RELEASE_STRING);
	printf("Copyright IBM Corp. 2007, 2008\n");
	exit(0);
}

/*
 * Store daemon's pid so it can be stopped
 */
void store_pid(void)
{
	FILE *filp;

	filp = fopen(pid_file, "w");
	if (!filp) {
		fprintf(stderr, "cannot open pid file %s: %s\n", pid_file,
			strerror(errno));
		if (foreground == 0)
			syslog(LOG_ERR,
			       "cannot open pid file %s: %s", pid_file,
				strerror(errno));
		exit(1);
	}
	fprintf(filp, "%d\n", getpid());
	fclose(filp);
}

/*
 * check that we don't try to start this daemon twice
 */
void check_if_started_twice()
{
	FILE *filp;
	int pid, rc;

	filp = fopen(pid_file, "r");
	if (filp) {
		rc = fscanf(filp, "%d", &pid);
		if (rc != 1) {
			fprintf(stderr, "Reading pid file failed. Abortig!\n");
			exit(1);
		}
		fprintf(stderr, "pid file %s still exists.\n", pid_file);
		fprintf(stderr, "This may indicate that an instance ");
		fprintf(stderr, "of this daemon is already running\n");
		if (foreground == 0) {
			syslog(LOG_ERR, "pid file %s still exists.\n",
			       pid_file);
			syslog(LOG_ERR, "This may indicate that an instance ");
			syslog(LOG_ERR, "of this daemon is already running\n");
		}
		exit(1);
	}
}

/*
 * clean up method
 */
void clean_up()
{
	if (foreground == 0)
		syslog(LOG_INFO, "terminated\n");
	remove(pid_file);
	reactivate_cpus();
	if (check_cmmfiles() == 0)
		cleanup_cmm();
	exit(1);
}

/*
 * end the deamon
 */
void kill_daemon(int a)
{
	if (foreground == 0)
		syslog(LOG_INFO, "shutting down\n");
	remove(pid_file);
	reactivate_cpus();
	if (check_cmmfiles() == 0)
		cleanup_cmm();
	exit(0);
}
/*
 * reload the daemon (for lsb compliance)
 */
void reload_daemon(int a)
{
	int temp_cpu, temp_mem;

	sem = 0;
	if (debug) {
		if (foreground == 1)
			printf("cpuplugd restarted\n");
		if (foreground == 0)
			syslog(LOG_INFO, "cpuplugd restarted\n");
	}
	/*
	 * Before we parse the configuration file again we have to safe
	 * the original values prior to startup. if we don't do this cpuplugd
	 * no longer know how many cpus the system had before the daemon
	 * was started and therefor can't restore theres in case it is stopped
	 */
	temp_cpu = num_cpu_start;
	temp_mem = cmm_pagesize_start;
	parse_configfile(&cfg, configfile);
	check_config(&cfg);
	check_max(&cfg);
	num_cpu_start = temp_cpu;
	cmm_pagesize_start = temp_mem;
	sem = 1;
}
/*
 * Set up for handling SIGTERM or SIGINT
 *
 * taken from mon_fsstatd.c
 */
void handle_signals(void)
{
	struct sigaction act;

	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	act.sa_handler = kill_daemon;
	if (sigaction(SIGTERM, &act, NULL) < 0) {
		fprintf(stderr, "sigaction( SIGTERM, ... ) failed - "
			"reason %s\n", strerror(errno));
		if (foreground == 0)
			syslog(LOG_ERR, "sigaction( SIGTERM, ... ) failed - "
				"reason %s\n", strerror(errno));
		exit(1);
	}
	if (sigaction(SIGINT, &act, NULL) < 0) {
		fprintf(stderr, "sigaction( SIGINT, ... ) failed - "
			"reason %s\n", strerror(errno));
		if  (foreground == 0)
			syslog(LOG_ERR, "sigaction( SIGINT, ... ) failed - "
				"reason %s\n", strerror(errno));
		exit(1);
	}
}

/*
 * signal handler for sighup. This is used to force the deamon to reload its
 * configuration file.
 * this feature is also required by a lsb compliant init script
 */
void handle_sighup(void)
{
	struct sigaction act;

	act.sa_flags = 0;
	sigemptyset(&act.sa_mask);
	act.sa_handler = reload_daemon;
	if (sigaction(SIGHUP, &act, NULL) < 0) {
		fprintf(stderr, "sigaction( SIGHUP, ... ) failed - "
			"reason %s\n", strerror(errno));
		if (foreground == 0)
			syslog(LOG_ERR, "sigaction( SIGHUP, ... ) failed - "
				"reason %s\n", strerror(errno));
		exit(1);
	}
}


/*
 * check if the current system operates above the maximum cpu
 * and memory limits and enforce the new limits
 */
void check_max(struct config *cfg)
{
	int cpuid;

	cpuid = 0;
	if (cpu && cfg->cpu_max > 0 && get_num_online_cpus() > cfg->cpu_max) {
		if (debug && foreground == 1) {
			printf("The number of online cpus is above"
				" the maximum and will "
				"be decreased.\n");
		}
		if (debug && foreground == 0) {
			syslog(LOG_INFO, "The number of online cpus"
					"is above the maximum and will"
				       " be decreased\n");
		}
		while (get_num_online_cpus() > cfg->cpu_max &&
			cpuid < MAX_CPU) {
			if (is_online(cpuid) != 1) {
				cpuid++;
				continue;
			}
			if (debug && foreground == 1)
				printf("cpu with id %d is currently "
				       "online and will be disabled\n",
				       cpuid);
			if (debug && foreground == 0)
				syslog(LOG_INFO, "CPU with id %d is "
				       "currently online and will be "
				       "disabled\n", cpuid);
			hotunplug(cpuid);
			cpuid++;
		}
	}
}

/* check if we are running in an LPAR environment.
 * this functions return 0 if we run inside an lpar and -1 otherwise
 */

int check_lpar()
{
	int rc;
	FILE *filp;
	size_t bytes_read;
	char buffer[2048];
	char *contains_vm;

	rc = -1;
	filp = fopen("/proc/cpuinfo", "r");
	if (!filp) {
		fprintf(stderr, "cannot open /proc/cpuinfo: %s \n"
			, strerror(errno));
		if (foreground == 0)
			syslog(LOG_ERR, "cannot open /proc/cpuinfo: %s\n"
				, strerror(errno));
		clean_up();
	}
	bytes_read = fread(buffer, 1, sizeof(buffer), filp);
	if (bytes_read == 0) {
		fprintf(stderr, "Reading /proc/cpuinfo failed:");
		fprintf(stderr, "%s\n", strerror(errno));
		if (foreground == 0)
			syslog(LOG_ERR, "Reading /proc/cpuinfo failed"
			":%s\n", strerror(errno));
		goto out;
	}
	 /* NUL-terminate the text */
	buffer[bytes_read] = '\0';
	contains_vm = strstr(buffer, "version = FF");
	if (contains_vm == NULL) {
		rc = 0;
		if (debug && foreground == 1)
			printf("Detected System running in LPAR mode\n");
		if (debug && foreground == 0)
			syslog(LOG_INFO, "Detected System running in"
					" LPAR mode\n");
		goto out;
	} else {
		if (debug && foreground == 1)
			printf("Detected System running in z/VM mode\n");
		if (debug && foreground == 0)
			syslog(LOG_INFO, "Detected System running in"
					" z/VM mode\n");
	}
out:
	fclose(filp);
	return rc;
}
