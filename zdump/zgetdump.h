/*
 *  header file for zgetdump
 *    Copyright IBM Corp. 2001, 2006.
 *    Author(s): Despina Papadopoulou
 */

/* This header file holds the architecture specific crash dump header */
#ifndef _ZGETDUMP_H
#define _ZGETDUMP_H

#include <sys/time.h>
#include <stdint.h>
#include <sys/types.h>

/* definitions (this has to match with vmdump.h of lcrash */

#define DUMP_MAGIC_S390     0xa8190173618f23fdULL  /* s390 magic number     */
#define DUMP_MAGIC_LKCD     0xa8190173618f23edULL  /* lkcd magic number     */
#define DUMP_MAGIC_LIVE     0xa8190173618f23cdULL  /* live magic number     */

#define S390_DUMP_HEADER_SIZE     4096
#define MAX_DUMP_VOLUMES          32
#define DUMP_ASM_MAGIC_NUMBER     0xdeaddeadULL    /* magic number            */

#define MIN(x, y) ((x) < (y) ? (x) : (y))

/*
 * Structure: s390_dump_header_t
 *  Function: This is the header dumped at the top of every valid s390 crash
 *            dump.
 */

typedef struct _s390_dump_header_s {
        /* the dump magic number -- unique to verify dump is valid */
	uint64_t             dh_magic_number;                    /* 0x000 */

        /* the version number of this dump */
	uint32_t             dh_version;                         /* 0x008 */

        /* the size of this header (in case we can't read it) */
	uint32_t             dh_header_size;                     /* 0x00c */

        /* the level of this dump (just a header?) */
	uint32_t             dh_dump_level;                      /* 0x010 */

        /* the size of a Linux memory page (4K, 8K, 16K, etc.) */
	uint32_t             dh_page_size;                       /* 0x014 */

        /* the size of all physical memory */
	uint64_t             dh_memory_size;                     /* 0x018 */

        /* the start of physical memory */
	uint64_t             dh_memory_start;                    /* 0x020 */

        /* the end of physical memory */
	uint64_t             dh_memory_end;                      /* 0x028 */

        /* the number of pages in this dump specifically */
	uint32_t             dh_num_pages;                       /* 0x030 */

        /* ensure that dh_tod and dh_cpu_id are 8 byte aligned */
	uint32_t             dh_pad;                             /* 0x034 */

        /* the time of the dump generation using stck */
	uint64_t             dh_tod;                             /* 0x038 */

        /* cpu id */
	uint64_t             dh_cpu_id;                          /* 0x040 */

        /* arch */
	uint32_t             dh_arch;                            /* 0x048 */

        /* volume number */
	uint32_t             dh_volnr;                           /* 0x04c */

        /* build arch */
	uint32_t             dh_build_arch;                      /* 0x050 */

        /* real mem size */
	uint64_t             dh_real_memory_size;                /* 0x054 */

        /* multi-volume dump indicator */
	uint8_t              dh_mvdump;                          /* 0x05c */

        /* fill up to 512 bytes */
	unsigned char        end_pad1[0x200-0x05d];              /* 0x05d */

        /* the dump signature to verify a multi-volume dump partition */
	uint64_t             dh_mvdump_signature;                /* 0x200 */

        /* the time the partition was prepared for multi-volume dump */
	uint64_t             dh_mvdump_zipl_time;                /* 0x208 */

        /* fill up to 4096 byte */
	unsigned char        end_pad2[0x1000-0x210];             /* 0x210 */

} __attribute__((packed))  s390_dump_header_t;

/*
 * Structure: s390_dump_end_marker_t
 *  Function: This end marker should be at the end of every valid s390 crash
 *            dump.
 */

typedef struct _s390_dump_end_marker_{
	char end_string[8];
	unsigned long long end_time;
} __attribute__((packed)) s390_dump_end_marker_t; 

/*
 * Structure: lkcd 4.1 dump header
 */

typedef struct _dump_header_s {
	uint64_t             dh_magic_number;
	uint32_t             dh_version;
	uint32_t             dh_header_size;
	uint32_t             dh_dump_level;
	uint32_t             dh_dump_page_size;
	uint64_t             dh_memory_size;
	uint64_t             dh_memory_start;
	uint64_t             dh_memory_end;
	uint32_t             dh_num_dump_pages;
	char                 dh_panic_string[0x100];
	struct timeval       dh_time;
	char                 dh_utsname_sysname[65];
	char                 dh_utsname_nodename[65];
	char                 dh_utsname_release[65];
	char                 dh_utsname_version[65];
	char                 dh_utsname_machine[65];
	char                 dh_utsname_domainname[65];
	void                *dh_current_task;
	uint32_t             dh_dump_compress;
	uint32_t             dh_dump_flags;
	uint32_t             dh_dump_device;
} dump_header_4_1_t;

struct dump_tool {
	char		magic[7];
	uint8_t		version;
	char		code[0xff7 - 0x8];
	uint8_t		force;
	uint64_t	mem;
} __attribute__ ((packed));

struct mvdump_param {
	uint16_t	devno;
	uint32_t	start_blk;
	uint32_t	end_blk;
	uint8_t		blocksize;
	uint8_t		end_sec;
	uint8_t		num_heads;
} __attribute__ ((packed));

struct mvdump_parm_table {
	uint64_t	timestamp;
	uint16_t	num_param;
	struct mvdump_param param[MAX_DUMP_VOLUMES];
} __attribute__ ((packed));

enum dump_type {
	IS_TAPE      = 0,
	IS_DASD      = 1,
	IS_MULT_DASD = 2,
};

enum devnode_type {
	IS_DEVICE    = 0,
	IS_PARTITION = 1,
	IS_NOBLOCK   = 2,
};

enum device_status {
	ONLINE    = 0,
	OFFLINE   = 1,
	UNDEFINED = 2,
};

enum device_signature {
	INVALID = 0,
	VALID   = 1,
	ACTIVE  = 2,
};

struct disk_info {
	dev_t device;
	enum device_status status;
	enum device_signature signature;
	off_t start_offset;
	uint64_t part_size;
	char bus_id[9];
};

#endif /* _ASM_VMDUMP_H */
