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

/* check if we are running in an LPAR environment.
 * this functions return 0 if we run inside an lpar and -1 otherwise
 */
int islpar(void)
{
	int rc;
	FILE *filp;
	size_t bytes_read;
	char buffer[2048];
	char *contains_vm;

	rc = -1;
	filp = fopen("/proc/cpuinfo", "r");
	if (!filp) {
		fprintf(stderr, "%s: Can not open /proc/cpuinfo: %s \n"
			, name, strerror(errno));
		return rc;
	}
	bytes_read = fread(buffer, 1, sizeof(buffer), filp);
	if (bytes_read == 0) {
		fprintf(stderr, "%s: Reading /proc/cpuinfo failed:", name);
		fprintf(stderr, "%s\n", strerror(errno));
		goto out;
	}
	buffer[bytes_read] = '\0';
	contains_vm = strstr(buffer, "version = FF");
	if (contains_vm == NULL) {
		rc = 0;
		if (verbose)
			printf("Detected System running in LPAR mode\n");
		goto out;
	} else {
		if (verbose)
			printf("Detected System running in z/VM mode\n");
	}
out:
	fclose(filp);
	return rc;
}

/* get a substring of a given source with fixed start, end and length */
char *substring(size_t start, size_t stop, const char *src, char *dst,
		size_t size)
{
	unsigned int count;

	count = stop - start;
	if (count >= --size)
		count = size;
	sprintf(dst, "%.*s", count, src + start);
	return dst;
}

/*
 * check whether we are started as root.
 * Return 0 if user is root, non-zero otherwise.
 */
int check_for_root(void)
{
	if (geteuid() != 0)
		return -1;
	else
		return 0;
}

/*
 * check if a given string is composed of hex numbers soley.
 * return 0 if true and -1 otherwise
 */
int ishex(char *cp)
{

	unsigned int i;
	char c;

	for (i = 0; i <= strlen(cp); i++) {
		c = cp[i];
		if (isxdigit(cp[i]))
			continue;
		else
			return -1;
	}
	return 0;
}

/* convert a string to lower case */
void strlow(char *s)
{
	while (*s) {
		*s = tolower(*s);
		s++;
	}
}

/*
 * write a string to a particular file
 */
int strwrt(char *string, char *file)
{
	FILE *filp;
	char path[4096];
	int rc;

	strncpy(path, file, sizeof(path));
	if (access(path, W_OK) == 0) {
		filp = fopen(path, "w");
		if (!filp) {
			fprintf(stderr,	"%s: Can not open %s :", name, path);
			fprintf(stderr, "%s\n", strerror(errno));
			exit(1);
		}
		rc = fputs(string, filp);
		fclose(filp);
		if (rc < 0) {
			fprintf(stderr, "%s: Failed to write %s "
				"into %s:", name, string, path);
			fprintf(stderr, "%s\n", strerror(errno));
			return -1;
		} else
			return 0;
	} else {
		fprintf(stderr,	"%s: Can not open %s :", name, path);
		fprintf(stderr, "%s\n", strerror(errno));
	}
	return -1;
}

/*
 * read a string from a particular file
 */
int strrd(char *string, char *file)
{
	FILE *filp;
	char path[4096];
	int rc;

	strncpy(path, file, sizeof(path));
	if (access(path, R_OK) == 0) {
		filp = fopen(path, "rb");
		if (!filp) {
			fprintf(stderr,	"%s: Can not open %s: ", name, file);
			fprintf(stderr, "%s\n", strerror(errno));
			exit(1);
		}
		rc = fread(string, 4096, 1, filp);
		fclose(filp);
		/*
		 *  special handling is required for
		 *  /sys/firmware/reipl/ccw/loadparm
		 *  since this file can be empty
		 */
		if (rc < 0 && strncmp(file,
			"/sys/firmware/reipl/ccw/loadparm",
			strlen(file)) == 0) {
			string[0] = '\0';
			return 0;
		}
		if (rc < 0) {
			fprintf(stderr, "%s: Failed to read from %s:", name,
					file);
			fprintf(stderr, "%s\n", strerror(errno));
			return -1;
		} else {
			if (string[strlen(string) - 1] == '\n')
				string[strlen(string) - 1] = 0;
			return 0;
		}
	} else {
		fprintf(stderr,	"%s: Can not open %s: ", name, file);
		fprintf(stderr, "%s\n", strerror(errno));
	}
	return -1;
}

/*
 * read an integer from a particular file
 */
int intrd(int *val, char *file)
{
	FILE *filp;
	char path[4096];
	int rc;

	strncpy(path, file, sizeof(path));
	if (access(path, R_OK) == 0) {
		filp = fopen(path, "r");
		if (!filp) {
			fprintf(stderr,	"%s: Can not open %s: ", name, file);
			fprintf(stderr, "%s\n", strerror(errno));
			exit(1);
		}
		rc = fscanf(filp, "%d", val);
		fclose(filp);
		if (rc < 0) {
			/*
			 * supress error messages when called via lsreipl
			 *
			 */
			if (strncmp(file, "/sys/firmware/reipl/ccw/loadparm",
				strlen(file)) >= 0)
					return -1;
			fprintf(stderr, "%s: Failed to read from %s: ", name,
				file);
			fprintf(stderr, "%s\n", strerror(errno));
			return -1;
		} else
			return 0;
	} else {
		fprintf(stderr,	"%s: Can not open %s: ", name, file);
		fprintf(stderr, "%s\n", strerror(errno));
	}
	return -1;
}

/*
 * write an integer to a particular file
 */
int intwrt(int val, char *file)
{
	FILE *filp;
	char path[4096];
	int rc;

	strncpy(path, file, sizeof(path));
	if (access(path, W_OK) == 0) {
		filp = fopen(path, "w");
		if (!filp) {
			fprintf(stderr,	"%s: Can not open %s :", name, path);
			fprintf(stderr, "%s\n", strerror(errno));
			exit(1);
		}
		rc = fprintf(filp, "%d\n", val);
		fclose(filp);
		if (rc < 0) {
			fprintf(stderr, "%s: Failed to write %d into %s: ",
				name, val, path);
			fprintf(stderr, "%s\n", strerror(errno));
			return -1;
		} else
			return 0;
	} else {
		fprintf(stderr,	"%s: Can not open %s :", name, path);
		fprintf(stderr, "%s\n", strerror(errno));
	}
	return -1;
}

/*
 * write an unformated string to a particular file. this is needed to specify
 * multiple vmcmd commands
 */
int ustrwrt(char *string, char *file)
{
	char path[4096];
	int rc, fd;

	strncpy(path, file, sizeof(path));
	if (access(path, W_OK) == 0) {
		fd = open(file, O_WRONLY);
		if (!fd) {
			fprintf(stderr,	"%s: Can not open %s : ", name, path);
			fprintf(stderr, "%s\n", strerror(errno));
			exit(1);
		}
		rc = write(fd, string, strlen(string));
		close(fd);
		if (rc < 0) {
			fprintf(stderr, "%s: Failed to write %s into %s: ",
				name, string, path);
			fprintf(stderr, "%s\n", strerror(errno));
			return -1;
		} else
			return 0;
	} else {
		fprintf(stderr,	"%s: Can not open %s : ", name, path);
		fprintf(stderr, "%s\n", strerror(errno));
	}
	return -1;
}
