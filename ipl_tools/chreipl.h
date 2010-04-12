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

extern int use_ccw;	/* ccw or fcp */
extern int verbose;
extern int bootprog;
extern int bootprog_set;
extern char loadparm[9];
extern int loadparm_set;
extern char devno[9];	/* device number */
extern int devno_set;
extern char wwpn[20];
extern int wwpn_set;
extern char lun[20];
extern int lun_set;
extern char device[9];
extern char devparent[9];
extern char partition[15];
extern int partition_set;
extern char saction[8];
extern char name[256];
extern int action;

#define IPL_TYPE_LEN_MAX	100
#define NSS_NAME_LEN_MAX	8

#define ACT_CCW		1
#define ACT_FCP		2
#define ACT_NODE	3

enum reipl_type {
	T_FCP,
	T_CCW,
	T_NSS
};

void parse_options(int argc, char **argv);
int islpar();
char *substring(size_t start, size_t stop, const char *src, char *dst,
		size_t size);
unsigned long strtohex(char *s);
int ishex(char *cp);
int check_for_root(void);
int isccwdev(const char *devno);
int isfcpdev(char *devno);
int get_fcp_devno(char *device, char *devno);
int get_lun(char *device, char *lun);
int get_wwpn(char *device, char *wwpn);
int ispartition(char *devno);
void print_usage_sa(char program_name[]);
void print_usage_ripl(char program_name[]);
int is_valid_case(char *c);
int is_valid_action(char *action);
void parse_shutdown_options(int argc, char **argv);
void strlow(char *s);
int get_ccw_devno(char *device);
int get_reipl_type(char *reipltype);
void parse_lsreipl_options(int argc, char **argv);
int get_ipl_type(char *reipltype);
int get_ipl_loadparam(void);
void print_ipl_settings(void);
int get_sa(char *action, char *file);
void parse_lsshut_options(int argc, char **argv);
void print_usage_lsshut(char program_name[]);
int strwrt(char *string, char *file);
int ustrwrt(char *string, char *file);
int strrd(char *string, char *file);
int intrd(int *val, char *file);
int intwrt(int val, char *file);
int get_ccw_dev(char *partition, char *device);
int get_fcp_dev(char *partition, char *device);
