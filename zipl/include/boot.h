/*
 * s390-tools/zipl/include/boot.h
 *   Functions to handle the boot loader data.
 *
 * Copyright IBM Corp. 2001, 2006.
 *
 * Author(s): Carsten Otte <cotte@de.ibm.com>
 *            Peter Oberparleiter <Peter.Oberparleiter@de.ibm.com>
 */

#ifndef BOOT_H
#define BOOT_H

#include "zipl.h"

#include <sys/types.h>

#include "disk.h"
#include "job.h"


/* Boot data structures for FBA disks */

struct boot_fba_locread {
	uint64_t locate;
	uint64_t read;
} __attribute__ ((packed));

struct boot_fba_locdata {
	uint8_t command;
	uint8_t dummy;
	uint16_t blockct;
	uint32_t blocknr;
} __attribute__ ((packed));

struct boot_fba_stage0 {
	uint64_t PSW;
	uint64_t READ;
	uint64_t TIC;
	uint64_t param1;
	uint64_t param2;
	struct boot_fba_locread locread[8];
	struct boot_fba_locdata locdata[8];
} __attribute__ ((packed));


/* Boot data structures for ECKD disks */

struct boot_eckd_ccw0 {
	uint8_t cmd;
	uint8_t address_hi;
	uint16_t address_lo;
	uint8_t flags;
	uint8_t pad;
	uint16_t count;
} __attribute__ ((packed));

struct boot_eckd_ccw1 {
	uint8_t cmd;
	uint8_t flags;
	uint16_t count;
	uint32_t address;
} __attribute__ ((packed));

struct boot_eckd_ssrt {
	struct boot_eckd_ccw0 seek;
	struct boot_eckd_ccw0 search;
	struct boot_eckd_ccw0 tic;
	struct boot_eckd_ccw0 read;
} __attribute__ ((packed));

struct boot_eckd_seekarg {
	uint16_t pad;
	uint16_t cyl;
	uint16_t head;
	uint8_t sec;
	uint8_t pad2;
} __attribute__ ((packed));

struct boot_eckd_stage0 {
	uint64_t psw;
	struct boot_eckd_ccw0 read;
	struct boot_eckd_ccw0 tic;
} __attribute__ ((packed)) ipleckd0_t;

struct boot_eckd_classic_stage1 {
	uint64_t param1;
	uint64_t param2;
	struct boot_eckd_ssrt ssrt[8];
	struct boot_eckd_seekarg seek[8];
} __attribute__ ((packed));

struct boot_eckd_compatible_stage1 {
	uint64_t param1;
	uint64_t param2;
	struct boot_eckd_ccw0 seek;
	struct boot_eckd_ccw0 search;
	struct boot_eckd_ccw0 tic;
	struct boot_eckd_ccw0 read[12];
	struct boot_eckd_seekarg seekarg;
} __attribute__ ((packed)) ipleckd1b_t;


/* Stage 2 boot menu parameter structure */

#define BOOT_MENU_ENTRIES		62

struct boot_stage2_params {
	uint16_t flag;
	uint16_t timeout;
	uint16_t banner;
	uint16_t config[BOOT_MENU_ENTRIES + 1];
};


/* Stage 3 bootloader parameter structure */

struct boot_stage3_params {
	uint64_t parm_addr;
	uint64_t initrd_addr;
	uint64_t initrd_len;
	uint64_t load_psw;
	uint64_t extra_parm;
};


/* Tape IPL bootloader parameter structure */

#define BOOT_TAPE_IPL_PARAMS_OFFSET	0x100

struct boot_tape_ipl_params {
	uint64_t parm_addr;
	uint64_t initrd_addr;
	uint64_t load_psw;
};

/* Partition parameter table for multi-volume dump */

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
	unsigned char	reserved[512 - sizeof(uint64_t) - sizeof(uint16_t) -
		(MAX_DUMP_VOLUMES * sizeof(struct mvdump_param))];
} __attribute__ ((packed));


int boot_check_data(void);
int boot_init_fba_stage0(struct boot_fba_stage0* stage0,
			 disk_blockptr_t* stage2_list,
			 blocknum_t stage2_count);
int boot_get_fba_stage2(void** data, size_t* size, struct job_data* job);
void boot_init_eckd_classic_stage0(struct boot_eckd_stage0* stage0);
int boot_init_eckd_classic_stage1(struct boot_eckd_classic_stage1* stage1,
				  disk_blockptr_t* stage2_list,
				  blocknum_t stage2_count);
void boot_init_eckd_compatible_stage0(struct boot_eckd_stage0* stage0);
int boot_init_eckd_compatible_stage1(
			struct boot_eckd_compatible_stage1* stage1,
			struct disk_info* info);
int boot_get_eckd_stage2(void** data, size_t* size, struct job_data* job);
int boot_get_stage3(void** buffer, size_t* bytecount, address_t parm_addr,
		    address_t initrd_addr, size_t initrd_len,
		    address_t image_addr, int extra_parm);
int boot_get_tape_ipl(void** data, size_t* size, address_t parm_addr,
		      address_t initrd_addr, address_t image_addr);
int boot_get_tape_dump(void** data, size_t* size, uint64_t mem);
int boot_get_eckd_dump_stage2(void** data, size_t* size, uint64_t mem);
int boot_get_eckd_mvdump_stage2(void** data, size_t* size, uint64_t mem,
				uint8_t force, struct mvdump_parm_table);
int boot_init_eckd_classic_dump_stage1(struct boot_eckd_classic_stage1* stage1,
				       struct disk_info* info);
int boot_init_eckd_compatible_dump_stage1(
				struct boot_eckd_compatible_stage1* stage1,
				struct disk_info* info,
				int mvdump_switch);
int boot_get_fba_dump_stage2(void** data, size_t* size, uint64_t mem);

#endif /* BOOT_H */
