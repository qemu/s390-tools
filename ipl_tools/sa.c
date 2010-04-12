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
 * The shutdown_actions directory /sys/firmware/shutdown_actions
 * contains the following files:
 * - on_halt
 * - on_poff
 * - on_reboot
 * - on_panic
 *
 * The shutdown_actions attributes can contain the shutdown actions
 * 'ipl', 'reipl', 'dump', 'stop' or 'vmcmd' (vmcmd is not available
 * when running in LPAR!)
 *
 * These values specify what should be done in case of halt, power off, reboot
 * or kernel panic event. Default for on_halt, on_poff and on_panic is 'stop'.
 * Default for reboot is 'reipl'.
 *
 * The attributes can be set by writing the appropriate string into the virtual
 * files. The vmcmd directory also contains the four files on_halt, on_poff,
 * on_reboot, and on_panic.
 *
 * All theses files can contain CP commands. For example, if CP commands should
 * be executed in case of a halt, the on_halt attribute in the vmcmd directory
 * must contain the CP commands and the on_halt attribute in the
 * shutdown_actions directory must contain the string 'vmcmd'. CP commands
 * written to the vmcmd attributes must be uppercase. You can specify multiple
 * commands using the newline character \n as separator.
 *
 * The maximum command line length is limited to 127 characters. For CP commands
 * that do not end or stop the virtual machine, halt, power off, and panic will
 * stop the machine after the command execution.
 *
 * For reboot, the system will be rebooted using the parameters specified under
 * /sys/firmware/reipl.
 */


 /* check is a valid case was specified. valid entries are
  * 'ipl', 'reipl', 'dump', 'stop' or 'vmcmd'
  */
int is_valid_case(char *action)
{
	int rc, length;

	length = strlen(action);
	rc = -1;
	if (action == NULL || (length < 4 || length > 6))
		return rc;
	if (strncmp(action, "halt", 4) >= 0 && length == 4)
		rc = 0;
	if (strncmp(action, "poff", 4) >= 0 && length == 4)
		rc = 1;
	if (strncmp(action, "reboot", 6) >= 0 && length == 6)
		rc = 2;
	if (strncmp(action, "panic", 5) >= 0 && length == 5) {
		fprintf(stderr,	"%s: Please use \"service dumpconf\" for "
			"configuring the panic action\n", name);
		exit(1);
	}
	return rc;
}

/* check is a valid action was specified */
int is_valid_action(char *action)
{
	int rc, length;

	length = strlen(action);
	rc = -1;

	if (action == NULL || (length < 3 || length > 5))
		return rc;
	if (strncmp(action, "ipl", strlen("ipl")) == 0 && length == 3)
		rc = 0;
	if (strncmp(action, "reipl", strlen("reipl")) == 0 && length == 5)
		rc = 1;
	if (strncmp(action, "dump", strlen("dump")) == 0 && length == 4)
		rc = 2;
	if (strncmp(action, "stop", strlen("stop")) == 0 && length == 4)
		rc = 3;
	if (strncmp(action, "vmcmd", strlen("vmcmd")) == 0 && length == 5)
		rc = 4;
	return rc;
}

/*
 * get the content  of /sys/firmware/shutdown_actions/[file]
 */
int get_sa(char *action, char *file)
{
	FILE *filp;
	char path[4096];
	int rc;

	sprintf(path, "/sys/firmware/shutdown_actions/%s", file);
	if (access(path, R_OK) == 0) {
		filp = fopen(path, "r");
		if (!filp) {
			fprintf(stderr,	"%s: Can not open /sys/firmware"
				"/shutdown_actions/%s: ", name, file);
			fprintf(stderr, "%s\n", strerror(errno));
			return -1;
		}
		rc = fscanf(filp, "%s", action);
		fclose(filp);
		if (rc < 0) {
			fprintf(stderr,	"%s: Can not open"
				"/sys/firmware/shutdown_actions/%s: ", name,
				file);
			fprintf(stderr, "%s\n", strerror(errno));
			return -1;
		} else {
			return 0;
		}
	} else {
		fprintf(stderr,	"%s: Can not open /sys/firmware/shutdown"
			"_actions/%s: ", name, file);
		fprintf(stderr, "%s\n", strerror(errno));
	}
	return -1;
}
