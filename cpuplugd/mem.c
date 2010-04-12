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
#include "cpuplugd.h"

/*
 * return page in and out as well as the page
 * swap in and out rate based on /proc/vmstat
 */
int get_vmstats(struct vm_info *v)
{
	FILE *filp;
	size_t bytes_read;
	char buffer[2048];
	char *match_pgpgout;
	char *match_pswpout;
	char *match_pgpgin;
	char *match_pswpin;
	int pgpgout;		/* page out */
	int pswpout;		/* page swap out */
	int pswpin;		/* page swap in */
	int pgpgin;		/* page in */
	int rc;

	filp = fopen("/proc/vmstat", "r");
	if (!filp) {
		fprintf(stderr,	"Can not open /proc/vmstat:");
		fprintf(stderr, "%s\n", strerror(errno));
		if (foreground == 0)
			syslog(LOG_ERR, "Can not open /proc/vmstat"
			       ":%s\n", strerror(errno));
		rc = -1;
		goto out;
	}
	bytes_read = fread(buffer, 1, sizeof(buffer), filp);
	/*
	 * Bail if read failed or the buffer isn't big enough
	 */
	if (bytes_read == 0) {
		fprintf(stderr,	"Reading /proc/vmstat failed:");
		fprintf(stderr, "%s\n", strerror(errno));
		if (foreground == 0)
			syslog(LOG_ERR, "Reading /proc/vmstat failed"
			":%s\n", strerror(errno));
		rc = -1;
		goto outc;
	}
	 /*
	  * NUL-terminate the text
	  */
	buffer[bytes_read] = '\0';
	match_pgpgout = strstr(buffer, "pgpgout");
	match_pswpout = strstr(buffer, "pswpout");
	match_pgpgin = strstr(buffer, "pgpgin");
	match_pswpin = strstr(buffer, "pswpin");
	if (match_pgpgout == NULL || match_pswpout == NULL ||
		match_pgpgin == NULL || match_pswpin == NULL) {
		rc = -1;
		goto outc;
	}
	sscanf(match_pgpgout, "pgpgout %d", &pgpgout);
	sscanf(match_pswpout, "pswpout %d", &pswpout);
	sscanf(match_pgpgin, "pgpgin %d", &pgpgin);
	sscanf(match_pswpin, "pswpin %d", &pswpin);
	v->pgpgout = pgpgout;
	v->pswpout = pswpout;
	v->pgpgin = pgpgin;
	v->pswpin = pswpin;
	rc = 0;
outc:
	fclose(filp);
out:
	return rc;
}

/*
 * The cmm_pages value defines the size of the balloon of blocked memory.
 * Increasing the value is removing memory from Linux, which is an memunplug.
 * Decreasing the value is adding memory back to Linux, which is memplug.
 */

/*
 *  increase number of pages permanently reserved
 */
int memunplug(int size)
{
	int old_size, new_size;
	FILE *filp;

	old_size = get_cmmpages_size();
	/*
	 * new value: previous value + size
	 */
	new_size = old_size + size;
	filp = fopen("/proc/sys/vm/cmm_pages", "w");
	if (!filp) {
		fprintf(stderr, "Can not open /proc/sys/vm/cmm_pages: %s\n",
				strerror(errno));
		if (foreground == 0)
			syslog(LOG_ERR, "Can not open /proc/sys/vm/cmm_pages"
			       ":%s\n", strerror(errno));
		return -1;
	}
	fprintf(filp, "%d\n", new_size);
	if (debug && foreground == 1)
		printf("changed number of pages permanently reserved "
		       "to  %d \n", new_size);
	if (debug && foreground == 0)
		syslog(LOG_INFO, "changed number of pages permanently"
			       " reserved to %d \n", new_size);
	fclose(filp);
	return 1;
}

/*
 *  decrease number of pages permanently reserved
 */
int memplug(int size)
{
	int old_size, new_size;
	FILE *filp;

	old_size = get_cmmpages_size();
	/*
	 * new value: previous value - size
	 */
	new_size = old_size - size;
	if (new_size <= 0)
		return -1;
	filp = fopen("/proc/sys/vm/cmm_pages", "w");
	if (!filp) {
		fprintf(stderr, "Can not open /proc/sys/vm/cmmpages: %s\n",
				strerror(errno));
		if (foreground == 0)
			syslog(LOG_ERR, "Can not open /proc/sys/vm/cmm_pages"
			       ":%s\n", strerror(errno));
		return -1;
	}
	fprintf(filp, "%d\n", new_size);
	if (debug && foreground == 1)
		printf("changed number of pages permanently reserved "
		       "to  %d \n", new_size);
	if (debug && foreground == 0)
		syslog(LOG_INFO, "changed number of pages permanently"
				"reserved to %d \n", new_size);
	fclose(filp);
	return 1;
}

/*
 *  override the value of cmm_pages
 */
int set_cmm_pages(int size)
{
	FILE *filp;

	filp = fopen("/proc/sys/vm/cmm_pages", "w");
	if (!filp) {
		fprintf(stderr, "Can not open /proc/sys/vm/cmmpages: %s\n",
				strerror(errno));
		if (foreground == 0)
			syslog(LOG_ERR, "Can not open /proc/sys/vm/cmm_pages"
			       ":%s\n", strerror(errno));
		return -1;
	}
	fprintf(filp, "%d\n", size);
	if (debug && foreground == 1)
		printf("changed number of pages permanently reserved "
		       "to  %d \n", size);
	if (debug && foreground == 0)
		syslog(LOG_INFO, "changed number of pages permanently"
				"reserved to %d \n", size);
	fclose(filp);
	return 1;
}
/*
 *  read number of pages permanently reserved
 */
int get_cmmpages_size()
{
	FILE *filp;
	int size;
	int rc;

	filp = fopen("/proc/sys/vm/cmm_pages", "r");
	if (!filp) {
		fprintf(stderr, "Can not open /proc/sys/vm/cmm_pages: %s\n",
				strerror(errno));
		if (foreground == 0)
			syslog(LOG_ERR, "Can not open /proc/sys/vm/cmm_pages"
			       ":%s\n", strerror(errno));
		return -1;
	}
	rc = fscanf(filp, "%d", &size);
	if (rc == 0) {
		fprintf(stderr, "Can not read /proc/sys/vm/cmm_pages: %s\n",
				strerror(errno));
		if (foreground == 0)
			syslog(LOG_ERR, "Can not read /proc/sys/vm/cmm_pages"
			       ":%s\n", strerror(errno));
		return -1;
	}
	fclose(filp);
	return size;
}



/*
 *  reset cmm pagesize to value we found prior to daemon startup
 */
int cleanup_cmm()
{
	FILE *filp;

	filp = fopen("/proc/sys/vm/cmm_pages", "w");
	if (!filp) {
		fprintf(stderr, "Can not open /proc/sys/vm/cmm_pages: %s\n",
				strerror(errno));
		if (foreground == 0)
			syslog(LOG_ERR, "Can not open /proc/sys/vm/cmm_pages"
			       ":%s\n", strerror(errno));
		return -1;
	}
	fprintf(filp, "%d", cmm_pagesize_start);
	if (debug && foreground == 1)
		printf("changed number of pages permanently reserved"
		       " to  %d \n", cmm_pagesize_start);
	if (debug && foreground == 0)
		syslog(LOG_INFO, "changed number of pages permanently "
				 " reserved to %d \n",
				cmm_pagesize_start);
	fclose(filp);
	return 1;
}




/*
 *  function to check if the cmm kernel module is loaded and the required
 *  files below /proc exit
 */
int check_cmmfiles(void)
{
	FILE *filp;

	filp = fopen("/proc/sys/vm/cmm_pages", "r");
	if (!filp)
		return -1;
	fclose(filp);
	return 0;
}


/*
 * return amount of *free* memory as reported by
 * /proc/meminfo
 */
int get_free_memsize()
{
	int size;
	int rc;
	FILE *filp;
	size_t bytes_read;
	char buffer[2048];
	char *match;

	size = 1;
	rc = 1;
	filp = fopen("/proc/meminfo", "r");
	if (!filp) {
		fprintf(stderr, "%s\n", strerror(errno));
		if (foreground == 0)
			syslog(LOG_ERR, "Can not open /proc/meminfo"
			       ":%s\n", strerror(errno));
		clean_up();
	}
	bytes_read = fread(buffer, 1, sizeof(buffer), filp);
	/*
	 * Bail if read failed or the buffer isn't big enough
	 */
	if (bytes_read == 0) {
		fprintf(stderr,	"Reading /proc/meminfo failed:");
		fprintf(stderr, "%s\n", strerror(errno));
		if (foreground == 0)
			syslog(LOG_ERR, "Reading /proc/meminfo failed"
			":%s\n", strerror(errno));
		clean_up();
	}
	buffer[bytes_read] = '\0';
	/*
	 * Locate the line that starts with
	 * "MemTotal".
	 */
	match = strstr(buffer, "MemFree");
	if (match == NULL) {
		fprintf(stderr, "No MemFree entry found in /proc/meminfo\n");
		clean_up();
	} else {
		/*
		 * Parse the line to extract
		 * the amount of memory
		 */
		rc = sscanf(match, "MemFree : %d", &size);
	}
	fclose(filp);
	/* KB to MB */
	if (rc == 1)
		return size / 1024;
	else
		return -1;
}




