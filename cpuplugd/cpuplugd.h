/*
 * Copyright IBM Corp 2007
 * Author: Hans-Joachim Picht <hans@linux.vnet.ibm.com>
 *
 * Linux for System z Hotplug Deamon
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

/*
 *  Precedence of C operators
 *  full list:
 *  http://www.imada.sdu.dk/~svalle/
 *  courses/dm14-2005/mirror/c/_7193_tabular246.gif
 *
 *  ()
 *  +-
 *  * /
 *  < >
 *  &
 *  |
 */
enum op_prio {
	OP_PRIO_NONE,
	OP_PRIO_OR,
	OP_PRIO_AND,
	/* greater and lower */
	OP_PRIO_CMP,
	OP_PRIO_ADD,
	OP_PRIO_MULT
};

enum operation {
	/* Leaf operators */
	OP_SYMBOL_LOADAVG,
	OP_SYMBOL_RUNABLE,
	OP_SYMBOL_CPUS,
	OP_SYMBOL_IDLE,
	OP_SYMBOL_APCR,
	OP_SYMBOL_SWAPRATE,
	OP_SYMBOL_FREEMEM,
	OP_CONST,
	/* Unary operators */
	OP_NEG,
	OP_NOT,
	/* Binary operators */
	OP_AND,
	OP_OR,
	OP_GREATER,
	OP_LESSER,
	OP_PLUS,
	OP_MINUS,
	OP_MULT,
	OP_DIV,
	/* ... */
	/*Variables which are eligible within rules*/
	VAR_LOAD,       /* loadaverage */
	VAR_RUN,        /* number of runable processes */
	VAR_ONLINE     /* number of online cpus */
};

struct symbols {
	double loadavg;
	double runable_proc;
	double onumcpus;
	double idle;
	double freemem;
	double apcr;
	double swaprate;
};

struct term {
	enum operation op;
	double value;
	struct term *left, *right;
};

/*
 * List of  argurments taken fromt the configuration file
 *
 */
struct config {
	int cpu_max;
	int cpu_min;
	int update;
	int cmm_max;
	int cmm_min;
	int cmm_inc;
	long long idle_ticks;
	struct term *hotplug;
	struct term *hotunplug;
	struct term *memplug;
	struct term *memunplug;
};

struct thread1_params {
	struct vmstat *vs;
	pthread_mutex_t pm;
};

struct vmstat {
	int bi;		/* Blocks received from a block device (blocks/s) */
	int bo;		/* Blocks sent to a block device (blocks/s) */
	int si;		/* Amount of memory swapped in from disk (/s) */
	int so;		/* Amount of memory swapped to disk (/s) */
	int available;	/* boolean flag to check if the computed values are
			   already available */
};


struct vm_info {
	unsigned long long pgpgout;		/* page out */
	unsigned long long pswpout;		/* page swap out */
	unsigned long long pswpin;		/* page swap in */
	unsigned long long pgpgin;		/* page in */
};


/*
 * used to query data from /proc/stat
 */
struct cpustat  {
	unsigned int cpu_info;
	unsigned int cpu_user;
	unsigned int cpu_nice;
	unsigned int cpu_idle;
	unsigned int cpu_iow;
	unsigned int cpu_irq;
	unsigned int cpu_softirq;
	unsigned int cpu_sys;
	unsigned int cpu_steal;
};


#define NAME "cpuplugd"
#define MAX_CPU 64 /* max amount of possible cpus */
#define PIDFILE "/var/run/cpuplugd.pid"
#define MAXRULES 100
#define USER_HERTZ  100
#define VMSTAT        0
#define SLABSTAT      0x00000004

extern int foreground;
extern int debug;
extern char *configfile;
extern int debug;        /* is verbose specified? */
extern int swaprate;
extern int apcr;	/* amount of page cache reads */
extern int memory;
extern int cpu;
extern int num_cpu_start; /* # of online cpus at the time of the startup */
extern int cmm_pagesize_start; /* cmm_pageize at the time of daemon startup */
extern struct config cfg;
extern int sem;

int get_numcpus();
int get_num_online_cpus();
int get_runable_proc();
long long get_idle_ticks();
float get_loadavg();
void clean_up();
void reactivate_cpus();
void parse_configfile(struct config *cfg, char *file);
void free_term(struct term *fn);
void print_term(struct term *fn);
struct term *parse_term(char **p, enum op_prio prio);
int eval_term(struct term *fn, struct symbols *symbols);
void *get_info(void *parameters);
int cleanup_cmm();
int hotplug(int cpuid);
int hotunplug(int cpuid);
int memplug(int size);
int memunplug(int size);
int is_online(int cpuid);
int get_cmmpages_size();
void parse_options(int argc, char **argv);
void check_if_started_twice();
void store_pid(void);
void handle_signals(void);
void handle_sighup(void);
int get_vmstats(struct vm_info *v);
unsigned long unit_convert(unsigned int size);
int check_cmmfiles(void);
int get_free_memsize();
void check_config(struct config *cfg);
void check_max(struct config *cfg);
int set_cmm_pages(int size);
int check_lpar();
int cpu_is_configured(int cpuid);
