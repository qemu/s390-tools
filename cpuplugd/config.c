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

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

#define __USE_ISOC99
#include <ctype.h>

#include "cpuplugd.h"

/*
 * return the int value of a variable which parse_config()
 * found within the configuration file
 */
static int parse_int_value(char *ptr)
{
	int value;
	unsigned int i;

	value = 0;
	if (ptr == NULL)
		return -1;
	sscanf(ptr, "%d", &value);
	if (value == 0) {
		fprintf(stderr, "parsing error\n");
		clean_up();
	}
	/*
	 * This is an extra check of the following scenario.
	 * A user specified 1O (1 and o) as the input. Normally
	 * atio() as well as scanf would return 1 in this case,
	 * but from a user perspective an error message is appreciated
	 */
	for (i = 0; i < strlen(ptr); i++) {
		if (isdigit(ptr[i])  == 0)
			return -1;
	}
		return value;
}

/*
 * parse a single line of the configuration file
 */
void parse_configline(struct config *cfg, char *line)
{
	char *token, *match, *name, *rvalue_temp1, *rvalue;
	char *save = NULL, *start, *stop;
	int i, j, rc, cmm_min, cpu_max;
	size_t len;

	rc = -1;
	cmm_min = -1;
	rvalue = NULL;
	/* parse line by line */
	for (token = strtok_r(line, "\n", &save);
	     token != NULL;
	     token = strtok_r(NULL, "\n", &save)) {
		match = strchr(token, '#');
		if (match)
			match[0] = 0;	/* Skip everything after a '#'. */
		char temp[strlen(token) + 1];
		for (i = j = 0; token[i] != 0; i++) /* Remove whitespace. */
			if (!isblank(token[i]) && !isspace(token[i]))
				temp[j++] = token[i];
		temp[j] = '\0';
		match = strchr(temp, '=');
		if (match == NULL)
			continue;
		match[0] = 0;		/* Separate name and right hand value */
		name = temp;		/* left side of = */
		rvalue_temp1 = match + 1;	/* right side of = */
		/*
		 * remove the double quotes
		 * example:  CPU_MIN="2"
		 */
		start = strchr(rvalue_temp1, '"'); /* points to first " */
		stop = strrchr(rvalue_temp1, '"'); /* points to last " */
		len = stop-start;
		char rvalue_temp2[len];
		if (start != NULL && stop != NULL && len > 0) {
			strncpy(rvalue_temp2, start+1, len-1);
			rvalue_temp2[len-1] = '\0';
			rvalue = (char *)&rvalue_temp2;
		} else {
			if (debug && foreground == 1)
				printf("the configuration file has "
					"syntax errors\n");
			if (debug && foreground == 0)
				syslog(LOG_INFO, "the configuration fle has "
						"syntax errors\n");
			clean_up();
		}

		/*
		 * copy hotplug and hotunplug rules to a struct
		 * for later evaluation
		 */
		if (!strncasecmp(name, "hotplug", strlen("hotplug"))) {
			if (debug && foreground == 1)
				printf("found the following rule: %s = %s\n",
				       name, rvalue);
			if (debug && foreground == 0)
				syslog(LOG_INFO, "found the following rule: "
				       "%s = %s\n", name, rvalue);
			cfg->hotplug = parse_term(&rvalue, OP_PRIO_NONE);
			if (rvalue[0] == '\0')
				continue;
			/* hotplug term has errors. */
			if (debug && foreground == 1)
				printf("rule has syntax errors\n");
			if (debug && foreground == 0)
				syslog(LOG_INFO, "rule has syntax errors\n");
			clean_up();
		}
		if (!strncasecmp(name, "hotunplug", strlen("hotunplug")))  {
			if (debug && foreground == 1)
				printf("found the following rule: %s = %s\n",
				       name, rvalue);
			if (debug && foreground == 0)
				syslog(LOG_INFO, "found the following rule: "
				       "%s = %s\n", name, rvalue);
			cfg->hotunplug = parse_term(&rvalue, OP_PRIO_NONE);
			if (rvalue[0] == '\0')
				continue;
			/* hotplug term has errors. */
			if (debug && foreground == 1)
				printf("rule has syntax errors\n");
			if (debug && foreground == 0)
				syslog(LOG_INFO, "rule has syntax errors\n");
			clean_up();
		}
		if (!strncasecmp(name, "memplug", strlen("memplug"))) {
			if (debug && foreground == 1)
				printf("found the following rule: %s = %s\n",
				       name, rvalue);
			if (debug && foreground == 0)
				syslog(LOG_INFO, "found the following rule: "
				       "%s = %s\n", name, rvalue);
			cfg->memplug = parse_term(&rvalue, OP_PRIO_NONE);
			if (rvalue[0] == '\0')
				continue;
			/* memplug term has errors. */
			if (debug && foreground == 1)
				printf("rule has syntax errors\n");
			if (debug && foreground == 0)
				syslog(LOG_INFO, "rule has syntax errors\n");
			clean_up();
		}
		if (!strncasecmp(name, "memunplug", strlen("memunplug")))  {
			if (debug && foreground == 1)
				printf("found the following rule: %s = %s\n",
				       name, rvalue);
			if (debug && foreground == 0)
				syslog(LOG_INFO, "found the following rule: "
				       "%s = %s\n", name, rvalue);
			cfg->memunplug = parse_term(&rvalue, OP_PRIO_NONE);
			if (rvalue[0] == '\0')
				continue;
			/* memunplug term has errors. */
			if (debug && foreground == 1)
				printf("rule has syntax errors\n");
			if (debug && foreground == 0)
				syslog(LOG_INFO, "rule has syntax errors\n");
			clean_up();
		}
		if (!strncasecmp(name, "update", strlen("update"))) {
			cfg->update = parse_int_value(rvalue);
			if (debug && foreground == 1)
				printf("found update value: %i\n",
				       cfg->update);
			if (debug && foreground == 0)
				syslog(LOG_INFO, "found update value: %i\n",
				       cfg->update);
			if (cfg->update == -1) {
				if (debug && foreground == 1) {
					fprintf(stderr, "parsing error at"
						" update\n");
					clean_up();
				}
			}
		}
		if (!strncasecmp(name, "cpu_min", strlen("cpu_min"))) {
			cfg->cpu_min = parse_int_value(rvalue);
			if (debug && foreground == 1)
				printf("found cpu_min value: %i\n",
				       cfg->cpu_min);
			if (debug && foreground == 0)
				syslog(LOG_INFO, "found cpu_min value: %i\n",
				       cfg->cpu_min);
			if (cfg->cpu_min == -1) {
				if (debug && foreground == 1) {
					fprintf(stderr, "parsing error at"
						" cpu_min\n");
					clean_up();
				}
			}
		}
		/*
		 * for cmm_min and cpu_max  we cannot use parse_int, because
		 * 0 is a valid input here
		 */
		if (!strncasecmp(name, "cpu_max", strlen("cpu_max"))) {
			rc =  sscanf(rvalue, "%d", &cpu_max);
			if (rc == 0 || cpu_max < 0) {
				fprintf(stderr, "parsing error at cpu_max\n");
				clean_up();
			}
			cfg->cpu_max = cpu_max;
			if (debug && foreground == 1) {
				if (cfg->cpu_max == -1)
					printf("found cpu_max value: %d\n", 0);
				else {
				printf("found cpu_max value: %i\n",
					cfg->cpu_max);
				}
			}
			if (debug && foreground == 0)
				syslog(LOG_INFO, "found cpu_max value: %i\n",
				       cfg->cpu_max);
			/*
			 * if cpu_max is 0, we use the overall number of cpus
			 */
			if (cfg->cpu_max == 0)
				cfg->cpu_max = get_numcpus();
		}
		if (!strncasecmp(name, "cmm_min", strlen("cmm_min"))) {
			rc = sscanf(rvalue, "%d", &cmm_min);
			if (rc == 0 || cmm_min < 0) {
				fprintf(stderr, "parsing error at cmm_min\n");
				clean_up();
			}
			cfg->cmm_min = cmm_min;
			if (debug && foreground == 1)
				printf("found cmm_min value: %i\n",
				       cfg->cmm_min);
			if (debug && foreground == 0)
				syslog(LOG_INFO, "found cmm_min value: %i\n",
				       cfg->cmm_min);
		}
		if (!strncasecmp(name, "cmm_max", strlen("cmm_max"))) {
			cfg->cmm_max = parse_int_value(rvalue);
			if (debug && foreground == 1)
				printf("found cmm_max value: %i\n",
				       cfg->cmm_max);
			if (debug && foreground == 0)
				syslog(LOG_INFO, "found cmm_max value: %i\n",
				       cfg->cmm_max);
			if (cfg->cmm_max == -1) {
				if (debug && foreground == 1) {
					fprintf(stderr, "parsing error at"
						" cmm_max\n");
					clean_up();
				}
			}
		}
		if (!strncasecmp(name, "cmm_inc", strlen("cmm_inc"))) {
			cfg->cmm_inc = parse_int_value(rvalue);
			if (debug && foreground == 1)
				printf("found cmm_inc value: %i\n",
				       cfg->cmm_inc);
			if (debug && foreground == 0)
				syslog(LOG_INFO, "found cmm_inc value: %i\n",
				       cfg->cmm_inc);
			if (cfg->cmm_inc == -1) {
				if (debug && foreground == 1) {
					fprintf(stderr, "parsing error at"
						" cmm_inc\n");
					clean_up();
				}
			}
		}
	}
}

/*
 * function used to parse the min and max values at the beginning of the
 * configuration file as well as the hotplug and hotunplug rules.
 */
void parse_configfile(struct config *cfg, char *file)
{
	FILE *filp;			/* file pointer */
	char linebuffer[2048];		/* current line */

	filp = fopen(file, "r");
	if (!filp) {
		fprintf(stderr, "Opening configuration file failed: %s\n",
				strerror(errno));
		clean_up();
	}
	while (fgets(linebuffer, sizeof(linebuffer)-1, filp) != NULL)
		parse_configline(cfg, linebuffer);
}


 /*
 * check if the required settings are found in the configuration file.
 * "autodetect" if cpu and/or memory hotplug configuration entries
 * where specified
 */
void check_config(struct config *cfg)
{
	int cpuid;
	int lpar_status, error_counter;

	lpar_status = check_lpar();
	if (cfg->cpu_max <= cfg->cpu_min && cfg->cpu_max != 0 && cpu != 0) {
		if (foreground == 1)
			fprintf(stderr, "cpu_max below or equal cpu_min,"
					" aborting.\n");
		if (foreground == 0)
			syslog(LOG_INFO, "cpu_max below or equal cpu_min,"
				"aborting\n");
		clean_up();
	}
	if (cfg->cpu_max < 0 || cfg->cpu_min < 0 || cfg->update < 0
		|| cfg->hotplug == NULL || cfg->hotunplug == NULL) {
			if (foreground == 1)
				fprintf(stderr, "No valid CPU hotplug "
					"configuration detected.\n");
			if (foreground == 0)
				syslog(LOG_INFO, "No valid CPU hotplug "
					"configuration detected\n");
		cpu = 0;
	} else {
		cpu = 1;
		if (debug) {
			if (foreground == 1)
				printf("Valid CPU hotplug configuration "
					"detected.\n");
			if (foreground == 0)
				syslog(LOG_INFO, "Valid CPU hotplug "
					"configuration detected\n");
		}
	}
	if (cfg->cmm_max < 0 || cfg->cmm_min < 0
		|| cfg->cmm_inc < 0 || cfg->memplug == NULL || cfg->update < 0
		|| cfg->memunplug == NULL) {
		if (foreground == 1)
			fprintf(stderr, "No valid memory hotplug "
				"configuration detected.\n");
		if (foreground == 0)
			syslog(LOG_INFO, "No valid memory hotplug "
				"configuration detected\n");
	} else {
		memory = 1;
		/*
		* check if all the necessary files exit below /proc
		*/
		if (check_cmmfiles() != 0 && lpar_status == -1) {
			if (foreground == 1)
				printf("Can not open /proc/sys/vm/cmm_pages\n"
					"The memory hotplug function will be "
					"disabled. \n");
			if (foreground == 0)
				syslog(LOG_INFO, "Can not open "
					"/proc/sys/vm/cmm_pages\n"
					" The memory hotplug function will be "
					"disabled. \n");
			memory = 0;
		}
		if (memory == 1  && lpar_status == -1) {
			if (foreground == 1 && debug)
				printf("Valid memory hotplug configuration "
					"detected.\n");
			if (foreground == 0 && debug)
				syslog(LOG_INFO, "Valid memory hotplug "
						"configuration detected\n");
		}
		if (memory == 1  && lpar_status == 0) {
			if (foreground == 1 && debug)
				printf("Valid memory hotplug configuration "
					"detected inside LPAR\n"
					"The memory hotplug function will be "
					"disabled. \n");

			if (foreground == 0 && debug)
				syslog(LOG_INFO, "Valid memory hotplug "
					"configuration detected "
					"inside LPAR\n"
					"The memory hotplug function will be "
					"disabled. \n");
		memory = 0;
		}
	}
	if (memory == 0 && cpu == 0) {
		printf("Exiting, because neither a valid cpu nor a valid memory"
				" hotplug configuration was found.\n");
		clean_up();
	}
	/*
	* save the number of online cpus and the cmm_pagesize at startup,
	* so that we can enable exactly the same amount when the daemon ends
	*/
	if (cpu) {
		num_cpu_start = get_num_online_cpus();
		if (debug && foreground == 0)
			syslog(LOG_INFO, "Daemon started with %d active cpus.",
				num_cpu_start);
		/*
		 * check that the initial number of cpus is not below the
		 * minimum
		 */
		if (num_cpu_start < cfg->cpu_min &&
			get_numcpus() >= cfg->cpu_min) {
			if (debug && foreground == 1) {
				printf("The number of online cpus is below"
					" the minimum and will "
					"be increased.\n");
			}
			if (debug && foreground == 0) {
				syslog(LOG_INFO, "The number of online cpus"
						"is below the minimum and will"
					       " be increased\n");
			}
			error_counter = 0;
			cpuid = 0;
			while (get_num_online_cpus() < cfg->cpu_min &&
				cpuid < get_numcpus()) {
				if (is_online(cpuid) == 1) {
					cpuid++;
					continue;
				}
				if (cpuid < get_numcpus()) {
					if (debug && foreground == 1)
						printf("cpu with id %d is"
							" currently offline and"
							" will be enabled\n",
						       cpuid);
					if (debug && foreground == 0)
						syslog(LOG_INFO, "CPU with id "
							"%d is currently"
							" offline and will be "
							"enabled\n", cpuid);
					if (hotplug(cpuid) == -1)
						error_counter++;
				}
				/*
				 * break because we tried at least to enable
				 * every cpu once, but failed
				 */
				if (error_counter == MAX_CPU) {
					printf("Failed to initialize the "
						"minimum amount of cpus. This "
						"probably means that you "
						"specified more cpus in the "
						"cpu_min variable than "
						"currently exit\n");
						clean_up();
				}
				cpuid++;

			}
		}
		if (cfg->cpu_min > get_numcpus()) {
			/*
			 * this check only works if nobody used the
			 * additional_cpus in the boot parameter section
			 */
			printf("The minimum amount of cpus is above the number"
					" of available cpus.\n");
			printf("Detected %d available cpus\n", get_numcpus());;
			clean_up();

		}
		if (get_num_online_cpus() < cfg->cpu_min) {
			if (debug && foreground == 1) {
				printf("Failed to set the number of online cpus"
					" to the minimum. Aborting.\n");
			}
			if (debug && foreground == 0) {
				syslog(LOG_INFO, "Failed to set the number of "
					"online cpus to the minimum. "
					"Aborting.\n");
			}
		clean_up();
		}
	}
	if (memory == 1) {
		/*
		 * check that the initial value of cmm_pages is not below
		 * cmm_min or above cmm_max
		 */
		cmm_pagesize_start = get_cmmpages_size();
		if (cmm_pagesize_start < cfg->cmm_min) {
			if (debug && foreground == 1) {
				printf("cmm_pages is below minimum and will "
						"be increased.\n");
			}
			if (debug && foreground == 0) {
				syslog(LOG_INFO, "cmm_pages is below minimum "
						"and will be increased\n");
			}
			set_cmm_pages(cfg->cmm_min);
		}
		if (cmm_pagesize_start > cfg->cmm_max) {
			if (debug && foreground == 1) {
				printf("cmm_pages is above the maximum and will"
						" be decreased.\n");
			}
			if (debug && foreground == 0) {
				syslog(LOG_INFO, "cmm_pages is above the "
						"maximum and will be"
						" decreased\n");
			}
			set_cmm_pages(cfg->cmm_max);
		}
	}
}

