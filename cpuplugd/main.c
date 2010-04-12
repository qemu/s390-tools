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
#include <setjmp.h>
#include "cpuplugd.h"

pthread_mutex_t  m = PTHREAD_MUTEX_INITIALIZER;
static jmp_buf buf;
struct sigaction action_signal;
sigset_t set;
/* save the initial values for the cleanup procedure */
int num_cpu_start;
int cmm_pagesize_start;
int memory;
int cpu;
struct config cfg;
int sem;

/*
 * this functions handles the sigfpe signal which we
 * might catch during rule evaluating
 */
void handler(int sig)
{
	signal(SIGFPE, handler);
	longjmp(buf, 1);
}

void eval_cpu_rules(struct config *cfg)
{
	struct symbols symbols;
	int cpu, nr_cpus;
	int onumcpus, runable_proc;
	long long idle_ticks;

	onumcpus = get_num_online_cpus();
	runable_proc = get_runable_proc();
	idle_ticks = get_idle_ticks();
	symbols.loadavg = (double) get_loadavg();
	symbols.onumcpus = (double) onumcpus;
	symbols.runable_proc = (double) runable_proc;
	symbols.idle = (double) (idle_ticks - cfg->idle_ticks) / cfg->update;
	cfg->idle_ticks = idle_ticks;
	nr_cpus = get_numcpus();

	/* only use this for development and testing */
	if (debug && foreground == 1) {
		printf("---------------------------------------------\n");
		printf("update_interval: %d s\n", cfg->update);
		printf("cpu_min: %d\n", cfg->cpu_min);
		printf("cpu_max: %d\n", cfg->cpu_max);
		printf("loadavg: %f \n", symbols.loadavg);
		printf("idle percent = %f\n", symbols.idle);
		printf("numcpus %d\n", nr_cpus);
		printf("runable_proc: %d\n", runable_proc);
		printf("---------------------------------------------\n");
		printf("onumcpus:   %d\n", onumcpus);
		printf("---------------------------------------------\n");
		if (cfg->hotplug) {
			printf("hotplug: ");
			print_term(cfg->hotplug);
		}
		printf("\n");
		if (cfg->hotunplug) {
			printf("hotunplug: ");
			print_term(cfg->hotunplug);
		}
		printf("\n");
		printf("---------------------------------------------\n");
	}
	/* Evaluate the hotplug rule. */
	if (cfg->hotplug && eval_term(cfg->hotplug, &symbols)) {
		/* check the cpu nr limit */
		if (onumcpus + 1 > cfg->cpu_max) {
			/* cpu limit reached */
			if (debug) {
				if (foreground == 1)
					printf("maximum cpu limit is "
					       "reached\n");
				if (foreground == 0)
					syslog(LOG_INFO, "maximum cpu"
					       " limit is reached\n");
			}
			return;
		}
		/* try to find a offline cpu */
		for (cpu = 0; cpu < nr_cpus; cpu++)
			if (is_online(cpu) == 0 && cpu_is_configured(cpu) != 0)
				break;
		if (cpu < nr_cpus) {
			if (debug && foreground == 1)
				printf("cpu with id %d is currently "
				       "offline and will be enabled\n",
				       cpu);
			if (debug && foreground == 0)
				syslog(LOG_INFO, "CPU with id %d is "
				       "currently offline and will be "
					"enabled\n", cpu);
			if (hotplug(cpu) == -1) {
				if (debug && foreground == 1)
					printf("unable to find a cpu which "
						"can be enabled\n");
				if (debug && foreground == 0)
					syslog(LOG_INFO, "unable to find a cpu"
						" which can be enabled\n");
			}
		} else {
			/*
			 * in case we tried to enable a cpu but this failed.
			 * this is the case if a cpu is deconfigured
			 */
			if (debug && foreground == 1)
				printf("unable to find a cpu which can be"
				       " enabled\n");
			if (debug && foreground == 0)
				syslog(LOG_INFO, "unable to find a cpu which"
					"can be enabled\n");
		}
		return;
	}
	/* Evaluate the hotplug rule. */
	if (cfg->hotunplug && eval_term(cfg->hotunplug, &symbols)) {
		/* check cpu nr limit */
		if (onumcpus  <= cfg->cpu_min) {
			if (debug) {
				if (foreground == 1)
					printf("minimum cpu limit "
					       "is reached\n");
				if (foreground == 0)
					syslog(LOG_INFO, "minimum cpu"
					       " limit is reached\n");
			}
			return;
		}
		/* try to find a online cpu */
		for (cpu = get_numcpus() - 1; cpu >= 0; cpu--) {
			if (is_online(cpu) != 0)
				break;
		}
		if (cpu > 0) {
			if (debug && foreground == 1)
				printf("cpu with id %d is currently "
				       "online and will be disabled\n",
				       cpu);
			if (debug && foreground == 0)
				syslog(LOG_INFO, "CPU with id %d is "
				       "currently online and will be "
				       "disabled\n", cpu);
			hotunplug(cpu);
		}
		return;
	}
}

void eval_mem_rules(struct config *cfg)
{
	struct symbols symbols;
	int cmmpages_size;
	int free_memory = get_free_memsize();

	if (free_memory == -1) {
		if (foreground == 1)
			printf("Failed to retrieve the free mem size: "
				"Aborting.\n");
		if (foreground == 0)
			syslog(LOG_INFO, "Failed to retrieve the free mem size:"
				" Aborting.\n");
		clean_up();
	}
	cmmpages_size = get_cmmpages_size();
	symbols.apcr = apcr;
	symbols.swaprate = swaprate;
	symbols.freemem = free_memory;

	/* only use this for development and testing */
	if (debug && foreground == 1) {
		printf("---------------------------------------------\n");
		printf("update_interval: %d s\n", cfg->update);
		printf("cmm_min: %d\n", cfg->cmm_min);
		printf("cmm_max: %d\n", cfg->cmm_max);
		printf("swaprate: %d\n", swaprate);
		printf("apcr: %d\n", apcr);
		printf("cmm_inc: %d\n", cfg->cmm_inc);
		printf("free memory: %d MB\n", free_memory);
		printf("---------------------------------------------\n");
		printf("cmm_pages: %d\n", cmmpages_size);
		printf("---------------------------------------------\n");
		if (cfg->memplug) {
			printf("memplug: ");
			print_term(cfg->memplug);
		}
		printf("\n");
		if (cfg->memunplug) {
			printf("memunplug: ");
			print_term(cfg->memunplug);
		}
		printf("\n");
		printf("---------------------------------------------\n");
	}
	/* Evaluate the memunplug rule. */
	if (cfg->memunplug && eval_term(cfg->memunplug, &symbols)) {
		/*
		 * case where cmm has asynchronously increased
		 * cmm_pages after cpuplugd reset it to cmm_max
		 * at cpuplugd startup.
		 *
		 */
		if (cmmpages_size > cfg->cmm_max) {
			if (debug) {
				if (foreground == 1)
					printf("Found cmm_pages above Limit. "
					       "Resetting value to %d\n"
					       , cfg->cmm_max);
				if (foreground == 0)
					syslog(LOG_INFO, "Found cmm_pages above"
						"Limit. Resetting value to %d\n"
					       , cfg->cmm_max);
			}
			set_cmm_pages(cfg->cmm_max);
			return;
		}
		/* check memory limit */
		if (cmmpages_size + cfg->cmm_inc > cfg->cmm_max) {
			if (debug) {
				if (foreground == 1)
					printf("maximum memory limit "
					       "is reached\n");
				if (foreground == 0)
					syslog(LOG_INFO, "maximum memory"
					       " limit is reached\n");
			}
			if (cmmpages_size < cfg->cmm_max) {
				/* if the next increment would exceed
				 * the maximum we advance to the
				 * maximum
				 */
				set_cmm_pages(cfg->cmm_max);
				return;
			}
		} else {
			 memunplug(cfg->cmm_inc);
			return;
		}
	}
	/* Evaluate the memplug rule. */
	if (cfg->memplug && eval_term(cfg->memplug, &symbols)) {
		 /* check memory limit */
		if (cmmpages_size - cfg->cmm_inc < cfg->cmm_min) {
			if (debug) {
				if (foreground == 1)
					printf("minimum memory limit "
					       "is reached\n");
				if (foreground == 0)
					syslog(LOG_INFO, "minimum memory"
					       " limit is reached\n");
				if (cmmpages_size > cfg->cmm_min) {
					/* if the next increment would exceed
					 * the minimum we advance to the
					 * minimum
					 */
					set_cmm_pages(cfg->cmm_min);
					return;
				}
			}
		} else {
			memplug(cfg->cmm_inc);
			return;
		}
	}
}

int main(int argc, char *argv[])
{
	int rc;
	/* This is needed to validate the config file */
	sem = 1;
	cfg.cpu_max = -1;
	cfg.cpu_min = -1;
	cfg.update = -1;
	cfg.cmm_min = -1;
	cfg.cmm_max = -1;
	cfg.cmm_inc = -1;
	cfg.memplug = NULL;
	cfg.memunplug = NULL;
	cfg.hotplug = NULL;
	cfg.hotunplug = NULL;
	/* parse the command line options */
	parse_options(argc, argv);
	/* make sure that the daemon is not started multiple times */
	check_if_started_twice();
	/* Store daemon pid also in foreground mode */
	handle_signals();
	handle_sighup();
	/* arguments taken from the configuration file */
	parse_configfile(&cfg, configfile);
	 /*check if the required settings are found in the configuration file*/
	check_config(&cfg);
	check_max(&cfg);
	if (!foreground) {
		rc = daemon(1, 0);
		if (rc < 0) {
			if (foreground == 1)
				fprintf(stderr, "Detach from terminal failed: "
					"%s\n", strerror(errno));
			if (foreground == 0)
				syslog(LOG_INFO, "Detach from terminal failed: "
					"%s\n", strerror(errno));
			clean_up();
		}
	}
	/* Store daemon pid */
	store_pid();
	/* thread to collect vmstat data */
	pthread_t thread_id1;
	struct vmstat *vs = malloc(sizeof(struct vmstat));
	if (vs == NULL) {
		if (foreground == 1)
			printf("Out of memory: Aborting.\n");
		if (foreground == 0)
			syslog(LOG_INFO, "Out of memory: Aborting.\n");
		clean_up();
	}
	/*
	* If the thread routine requires multiple arguments, they must be
	* passed bundled up in an array or a structure
	*/
	struct thread1_params t1args;
	t1args.vs = vs;
	t1args.pm = m;
	if (pthread_create(&thread_id1, NULL, &get_info, &t1args)) {
		if (foreground == 1)
			printf("Failed to start thread.\n");
		if (foreground == 0)
			syslog(LOG_INFO, "Failed to start thread\n");
	}
	/* ensure that reliable data from vmstat is gathered */
	while (memory == 1 && vs->available == 0)
		sleep(1);

	/* main loop */
	cfg.idle_ticks = get_idle_ticks();
	while (1) {
		if (sem == 0) {
			sleep(cfg.update);
			continue;
		}
		if (setjmp(buf) == 0) {

			/* install signal handler for floating point
			 * exceptions
			 */
			sigemptyset(&set);
			sigaddset(&set, SIGFPE);
			action_signal.sa_flags = 0;
			sigemptyset(&action_signal.sa_mask);
			action_signal.sa_handler = handler;
			sigaction(SIGFPE, &action_signal, NULL);
			/* Run code that may signal failure via longjmp. */
			if (cpu == 1)
				eval_cpu_rules(&cfg);
			if (memory == 1)
				eval_mem_rules(&cfg);
			sleep(cfg.update);
		} else {
			sleep(cfg.update);
			sigprocmask(SIG_UNBLOCK, &set , NULL);
			continue;
		}
	}
	return 0;
}
