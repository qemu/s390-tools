/*
 * s390-tools/zipl/include/zipl.h
 *   zSeries Initial Program Loader tool.
 *
 * Copyright IBM Corp. 2001, 2006.
 *
 * Author(s): Carsten Otte <cotte@de.ibm.com>
 *            Peter Oberparleiter <Peter.Oberparleiter@de.ibm.com>
 */

#ifndef ZIPL_H
#define ZIPL_H

#include <stdint.h>

#define ZIPL_MAGIC			"zIPL"
#define ZIPL_MAGIC_SIZE			4
#define DISK_LAYOUT_ID			0x00000001

#define ZIPL_STAGE2_LOAD_ADDRESS	0x2000
#define ZIPL_STAGE3_ENTRY_ADDRESS	0xa028LL
#define DEFAULT_PARMFILE_ADDRESS	0x1000LL
#define DEFAULT_STAGE3_ADDRESS		0xa000LL
#define DEFAULT_IMAGE_ADDRESS		0x10000LL
#define DEFAULT_RAMDISK_ADDRESS 	0x800000LL

#define PSW_ADDRESS_MASK		0x000000007fffffffLL
#define PSW_LOAD			0x0008000080000000LL
#define PSW_DISABLED_WAIT		0x000a000000000000LL

#define KERNEL_HEADER_SIZE		65536
#define BOOTMAP_FILENAME		"bootmap"
#define BOOTMAP_TEMPLATE_FILENAME	"bootmap_temp.XXXXXX"

#define ZIPL_CONF_VAR			"ZIPLCONF"
#define ZIPL_DEFAULT_CONF		"/etc/zipl.conf"

#define FSDUMP_IMAGE	STRINGIFY(ZFCPDUMP_DIR) "/" STRINGIFY(ZFCPDUMP_IMAGE)
#define FSDUMP_RAMDISK	STRINGIFY(ZFCPDUMP_DIR) "/" STRINGIFY(ZFCPDUMP_RD)

#define MAX_DUMP_VOLUMES		32

/* Internal component load address type */
typedef uint64_t address_t;

/* Type for address calculations */
#define VOID_ADD(ptr, offset)	((void *) (((unsigned long) ptr) + \
				((unsigned long) offset)))

/* Call a function depending on the value of dry_run and return either the
 * resulting return code or 0. */
#define	DRY_RUN_FUNC(x)	(dry_run ? 0 : (x))

extern int verbose;
extern int interactive;
extern int dry_run;

#endif /* not ZIPL_H */
