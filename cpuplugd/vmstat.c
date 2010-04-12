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
#include <linux/param.h>
#include <assert.h>
#include <fcntl.h>
#include <limits.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/dir.h>
#include <dirent.h>
#include <pthread.h>
#include "cpuplugd.h"

int swaprate;
int apcr;

static unsigned long dataUnit = 1024;
static int statMode = VMSTAT;
static const int Hertz = USER_HERTZ;	/* clock tick frequency */

/*
 * conversion function as used in vmstat.c (procps package)
 */
unsigned long unit_convert(unsigned int size)
{
	float cvSize;
	cvSize = (float)size/dataUnit*((statMode == SLABSTAT) ? 1 : 1024);
	return (unsigned long) cvSize;
}

/*
 * return the values from the "cpu" line within
 * /proc/stat
 */
void get_cpu_stats(struct cpustat *s)
{
	FILE *filp;
	size_t bytes_read;
	char buffer[2048];
	char *match;
	filp = fopen("/proc/stat", "r");
	if (!filp) {
		fprintf(stderr,	"Can not open /proc/stat:");
		fprintf(stderr, "%s\n", strerror(errno));
		if (foreground == 0)
			syslog(LOG_ERR, "Can not open /proc/stat"
			       ":%s\n", strerror(errno));
	} else {
		bytes_read = fread(buffer, 1, sizeof(buffer), filp);
		fclose(filp);
		/*
		 * Bail if read failed or the buffer isn't big enough
		 */
		if (bytes_read == 0) {
			fprintf(stderr,	"Reading /proc/stat failed:");
			fprintf(stderr, "%s\n", strerror(errno));
			if (foreground == 0)
				syslog(LOG_ERR, "Reading /proc/stat failed"
				":%s\n", strerror(errno));
		}
		/*
		 * NUL-terminate the text
		 */
		buffer[bytes_read] = '\0';
		match = strstr(buffer, "cpu");
		if (match != NULL) {
			/*
			 * cat /proc/stat|grep cpu looks like this:
			 * cpu  924432 26847 684506 8506417 117599 0 16305 0
			 *
			 * The very first "cpu" line aggregates the numbers in
			 * all of the other "cpuN" lines.
			 *
			 * These numbers identify the amount of time the CPU has
			 * spent performing different kinds of work.
			 * Time units are in USER_HZ (typically hundredths of
			 * a second).
			 *
			 * The meanings of the columns are as follows, from left
			 * to right:
			 *
			 * user: normal processes executing in user mode
			 * nice: niced processes executing in user mode
			 * system: processes executing in kernel mode
			 * idle: twiddling thumbs
			 * iowait: waiting for I/O to complete
			 * irq: servicing interrupts
			 * softirq: servicing softirqs
			 * steal: the cpu time spent in involuntary wait
			 */
			sscanf(match, "cpu  %du %du %du %du %du %du %du %du",
					&s->cpu_user,
					&s->cpu_nice,
					&s->cpu_sys,
					&s->cpu_idle,
					&s->cpu_iow,
					&s->cpu_irq,
					&s->cpu_softirq,
					&s->cpu_steal);
		}
	}
}


/*
 * function used to return the current si,so,bi and bo
 * values as known from the vmstat utility
 *
 */
void *get_info(void *parameters)
{
	struct thread1_params *p = (struct thread1_params *) parameters;
	struct vmstat *vs = p->vs;
	pthread_mutex_t job_queue_mutex = p->pm;

	struct vm_info *v = malloc(sizeof(struct vm_info));
	struct cpustat *s = malloc(sizeof(struct cpustat));
	/* values from /proc/vmstat */
	unsigned long pgpgin[2];
	unsigned long pgpgout[2];
	unsigned long pswpin[2];
	unsigned long pswpout[2];
	/* handle idle ticks running backwards */
	int debt;
	/* values from /proc/stat */
	unsigned int cpu_user[2];
	unsigned int cpu_nice[2];
	unsigned int cpu_idle[2];
	unsigned int cpu_iow[2];
	unsigned int cpu_irq[2];
	unsigned int cpu_softirq[2];
	unsigned int cpu_sys[2];
	unsigned int cpu_steal[2];
	/* magic */
	unsigned int divo2;
	unsigned int div;
	unsigned int duse;
	unsigned int dsys;
	unsigned int didl;
	unsigned int diow;
	unsigned int dstl;
	/* variable to toggle between current & previous value */
	unsigned int tog;
	unsigned int sleep_half;
	int sleep_time;
	/* page size */
	unsigned long kb_per_page = sysconf(_SC_PAGESIZE) / 1024ul;

	debt = 0;
	tog = 0;
	sleep_time = 10;
	sleep_half = (sleep_time / 2);
	/* receive /proc/stat values */
	get_cpu_stats(s);
	*cpu_user = s->cpu_user;
	*cpu_nice = s->cpu_nice;
	*cpu_irq = s->cpu_irq;
	*cpu_softirq = s->cpu_softirq;
	*cpu_idle = s->cpu_idle;
	*cpu_steal = s->cpu_steal;
	*cpu_sys = 0;
	*cpu_iow = 0; /* XXX */
	/* receive /proc/vmstat values */
	get_vmstats(v);
	*pgpgin = v->pgpgin;
	*pgpgout = v->pgpgout;
	*pswpin = v->pswpin;
	*pswpout = v->pswpout;
	/*
	 * unknown, algorithm based on vmstat.c
	 * implementation in the procps package
	 */
	duse = *cpu_user + *cpu_nice;
	dsys = *cpu_sys + *cpu_irq + *cpu_softirq;
	didl = *cpu_idle;
	diow = *cpu_iow;
	dstl = *cpu_steal;
	div = duse+dsys+didl+diow+dstl;
	divo2 = div/2UL;
	/* more magic from vmstat.c */
	pthread_mutex_lock(&job_queue_mutex);
	vs->si = (unsigned) (*pswpin * unit_convert(kb_per_page)
			* Hertz + divo2) / div;
	vs->so = (unsigned) (*pswpout * unit_convert(kb_per_page)
			* Hertz + divo2) / div;
	vs->bi = (unsigned) (*pgpgin * Hertz + divo2) / div;
	vs->bo = (unsigned) (*pgpgout * Hertz + divo2) / div;

	/* swaprate = vs->bi+vs->bo; */
	swaprate = vs->si+vs->so;
	apcr = vs->bi+vs->bo;
	pthread_mutex_unlock(&job_queue_mutex);
	/*printf("*********\n");
	printf("pgpgin: %4u\n", vs->bi);
	printf("pgpgout: %4u\n", vs->bo);
	printf("pswpin: %5u\n", vs->si);
	printf("pswpout: %5u\n", vs->so);
	*/
	while (1) {
		sleep(sleep_time);
		tog = !tog;
		get_vmstats(v);
		pgpgin[tog] = v->pgpgin;
		pgpgout[tog] = v->pgpgout;
		pswpin[tog] = v->pswpin;
		pswpout[tog] = v->pswpout;
		get_cpu_stats(s);
		cpu_user[tog] = s->cpu_user;
		cpu_nice[tog] = s->cpu_nice;
		cpu_irq[tog] = s->cpu_irq;
		cpu_softirq[tog] = s->cpu_softirq;
		cpu_idle[tog] = s->cpu_idle;
		cpu_steal[tog] = s->cpu_steal;
		duse = cpu_user[tog]-cpu_user[!tog]
			+ cpu_nice[tog]-cpu_nice[!tog];
		dsys = cpu_sys[tog]-cpu_sys[!tog] + cpu_irq[tog]-cpu_irq[!tog]
			+ cpu_softirq[tog]-cpu_softirq[!tog];
		didl = cpu_idle[tog]-cpu_idle[!tog];
		diow = cpu_iow[tog]-cpu_iow[!tog];
		dstl = cpu_steal[tog]-cpu_steal[!tog];
		/* idle can run backwards for a moment -- kernel "feature" */
		if (debt) {
			didl = (int)didl + debt;
			debt = 0;
		}
		if ((int)didl < 0) {
			debt = (int)didl;
			didl = 0;
		}
		div = duse+dsys+didl+diow+dstl;
		divo2 = div/2UL;

		pthread_mutex_lock(&job_queue_mutex);
		vs->si = (unsigned) (((pswpin[tog] - pswpin[!tog]) *
				unit_convert(kb_per_page) + sleep_half)
				/ sleep_time);
		vs->so = (unsigned) (((pswpout[tog] - pswpout[!tog]) *
			unit_convert(kb_per_page) + sleep_half)
			/ sleep_time);

		vs->bi = (unsigned) (((pgpgin[tog] - pgpgin[!tog]) +
			sleep_half) / sleep_time);
		vs->bo = (unsigned) (((pgpgout[tog] - pgpgout[!tog])
			+ sleep_half) / sleep_time);

		swaprate = vs->si+vs->so;
		apcr = vs->bi+vs->bo;
		/* swaprate = vs->bi+vs->bo; */
		/*
		printf("------------------------\n");
		printf("pgpgin: %4u\n", vs->bi);
		printf("pgpgout: %4u\n", vs->bo);
		printf("pswpin: %5u\n", vs->si);
		printf("pswpout: %5u\n", vs->so);
		*/
		pthread_mutex_unlock(&job_queue_mutex);
		if (vs->available != 1)
			vs->available = 1;
	}
	return NULL;
}
