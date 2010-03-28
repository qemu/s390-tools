/*
 * s390-tools/zipl/src/boot.c
 *   Functions to handle the boot loader data.
 *
 * Copyright IBM Corp. 2001, 2006.
 *
 * Author(s): Carsten Otte <cotte@de.ibm.com>
 *            Peter Oberparleiter <Peter.Oberparleiter@de.ibm.com>
 */

#include "boot.h"

#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "error.h"
#include "misc.h"

/* Include declarations for boot_data symbols. */
#include "../boot/data.h"

#define DATA_SIZE(x)	((size_t) (&_binary_##x##_bin_size))
#define DATA_ADDR(x)	(&_binary_##x##_bin_start)

static int stage2_max_size = 4096;


/* Check sizes of internal objects. Return 0 if everything is correct,
 * non-zero otherwise. */
int
boot_check_data(void)
{
	if (DATA_SIZE(fba0) !=
	    sizeof(struct boot_fba_stage0)) {
		error_reason("Size mismatch of stage 0 loader for disks with "
			     "FBA layout");
		return -1;
	}
	if (DATA_SIZE(eckd0) !=
	    sizeof(struct boot_eckd_stage0)) {
		error_reason("Size mismatch of stage 0 loader for disks with "
			     "ECKD layout");
		return -1;
	}
	if (DATA_SIZE(eckd1a) !=
	    sizeof(struct boot_eckd_classic_stage1)) {
		error_reason("Size mismatch of stage 1 loader for disks with "
			     "ECKD classic layout");
		return -1;
	}
	if (DATA_SIZE(eckd1b) !=
	    sizeof(struct boot_eckd_compatible_stage1)) {
		error_reason("Size mismatch of stage 1 loader for disks with "
			     "ECKD compatible layout");
		return -1;
	}
	return 0;
}


/* Create a stage 3 loader in memory.
 * Upon success, return 0 and set BUFFER to point to the data buffer and set
 * BYTECOUNT to contain the loader size in bytes. Return non-zero otherwise. */
int
boot_get_stage3(void** buffer, size_t* bytecount, address_t parm_addr,
		address_t initrd_addr, size_t initrd_len, address_t image_addr,
		int extra_parm)
{
	struct boot_stage3_params params;
	void* data;

	if (image_addr != (image_addr & PSW_ADDRESS_MASK)) {
		error_reason("Kernel image load address to high (31 bit "
			     "addressing mode)");
		return -1;
	}
	/* Get memory */
	data = misc_malloc(DATA_SIZE(stage3));
	if (data == NULL)
		return -1;
	/* Prepare params section */
	params.parm_addr = (uint64_t) parm_addr;
	params.initrd_addr = (uint64_t) initrd_addr;
	params.initrd_len = (uint64_t) initrd_len;
	params.load_psw = (uint64_t) (image_addr | PSW_LOAD);
	params.extra_parm = (uint64_t) extra_parm;
	/* Initialize buffer */
	memcpy(data, DATA_ADDR(stage3), DATA_SIZE(stage3));
	memcpy(data, &params, sizeof(struct boot_stage3_params));
	*buffer = data;
	*bytecount = DATA_SIZE(stage3);
	return 0;
}


#define FBA_FLAG_CC 0x0000000040000000LL

int
boot_init_fba_stage0(struct boot_fba_stage0* stage0,
		     disk_blockptr_t* stage2_list, blocknum_t stage2_count)
{
	blocknum_t i;

	/* Initialize stage 0 data */
	memcpy(stage0, DATA_ADDR(fba0), sizeof(struct boot_fba_stage0));
	/* Fill in blocklist for stage 2 loader */
	if (stage2_count > 8) {
		error_reason("Not enough room for FBA stage 2 loader "
			     "(try larger block size)");
		return -1;
	}
	for (i=0; i < stage2_count; i++) {
		stage0->locdata[i].blocknr =
			(uint32_t) stage2_list[i].linear.block;
	}
	/* Terminate CCW chain */
	stage0->locread[i - 1].read &= ~FBA_FLAG_CC;
	return 0;
}


void
boot_init_eckd_classic_stage0(struct boot_eckd_stage0* stage0)
{
	memcpy(stage0, DATA_ADDR(eckd0), sizeof(struct boot_eckd_stage0));
	/* Fill in size of stage 1 loader */
	stage0->read.count = sizeof(struct boot_eckd_classic_stage1);
}


void
boot_init_eckd_compatible_stage0(struct boot_eckd_stage0* stage0)
{
	memcpy(stage0, DATA_ADDR(eckd0), sizeof(struct boot_eckd_stage0));
	/* Fill in size of stage 1 loader */
	stage0->read.count = sizeof(struct boot_eckd_compatible_stage1);
}


#define ECKD_CCW_FLAG_CC	0x40
#define ECKD_CCW_FLAG_SLI	0x20

int
boot_init_eckd_classic_stage1(struct boot_eckd_classic_stage1* stage1,
		     disk_blockptr_t* stage2_list, blocknum_t stage2_count)
{
	blocknum_t i;

	memcpy(stage1, DATA_ADDR(eckd1a),
	       sizeof(struct boot_eckd_classic_stage1));
	/* Fill in blocklist for stage 2 loader */
	if (stage2_count > 8) {
		error_reason("Not enough room for ECKD stage 2 loader "
			     "(try larger block size)");
		return -1;
	}
	for (i=0; i < stage2_count; i++) {
		stage1->ssrt[i].read.count 	= stage2_list[i].chs.size;
		stage1->seek[i].cyl		= stage2_list[i].chs.cyl;
		stage1->seek[i].head		= stage2_list[i].chs.head |
			((stage2_list[i].chs.cyl >> 12) & 0xfff0);
		stage1->seek[i].sec		= stage2_list[i].chs.sec;
		stage1->ssrt[i].read.address_lo	= ZIPL_STAGE2_LOAD_ADDRESS +
						  i * stage2_list[i].chs.size;
		stage1->ssrt[i].read.flags	= ECKD_CCW_FLAG_CC |
						  ECKD_CCW_FLAG_SLI;
	}
	/* Terminate CCW chain */
	stage1->ssrt[i - 1].read.flags &= ~ECKD_CCW_FLAG_CC;
	return 0;
}


int
boot_init_eckd_classic_dump_stage1(struct boot_eckd_classic_stage1* stage1,
				   struct disk_info* info)
{
	blocknum_t blocks;
	blocknum_t i;

	memcpy(stage1, DATA_ADDR(eckd1a),
	       sizeof(struct boot_eckd_classic_stage1));
	blocks = (DATA_SIZE(eckd2dump) +
			info->phy_block_size - 1) / info->phy_block_size;
	if (blocks > 8) {
		error_reason("Not enough room for ECKD stage 2 dump record "
			     "(try larger block size)");
		return -1;
	}
	/* Fill in block list of stage 2 dump record */
	for (i=0; i < blocks; i++) {
		int cyl;

		stage1->ssrt[i].read.count = info->phy_block_size;
		stage1->ssrt[i].read.address_lo = ZIPL_STAGE2_LOAD_ADDRESS +
						  i * info->phy_block_size;
		cyl = disk_cyl_from_blocknum(i + info->geo.start, info);
		stage1->seek[i].cyl = cyl;
		stage1->seek[i].head =
			disk_head_from_blocknum(i + info->geo.start, info) |
			((cyl >> 12) & 0xfff0);
		stage1->seek[i].sec =
			disk_sec_from_blocknum(i + info->geo.start, info);
	}
	/* Terminate CCW chain */
	stage1->ssrt[i - 1].read.flags &= ~ECKD_CCW_FLAG_CC;
	return 0;
}


int
boot_init_eckd_compatible_stage1(struct boot_eckd_compatible_stage1* stage1,
				 struct disk_info* info)
{
	blocknum_t blocks;
	blocknum_t i;

	memcpy(stage1, DATA_ADDR(eckd1b),
	       sizeof(struct boot_eckd_compatible_stage1));
	/* Fill in blocklist for stage 2 loader */
	blocks = (stage2_max_size + info->phy_block_size - 1) /
		 info->phy_block_size;
	if (blocks > 12) {
		error_reason("Not enough room for ECKD stage 2 loader "
			     "(try larger block size)");
		return -1;
	}
	for (i=0; i < blocks; i++) {
		stage1->read[i].count		= info->phy_block_size;
		stage1->read[i].address_lo	= ZIPL_STAGE2_LOAD_ADDRESS +
						  i * info->phy_block_size;
	}
	/* Terminate CCW chain */
	stage1->read[i - 1].flags &= ~ECKD_CCW_FLAG_CC;
	return 0;
}


int
boot_get_tape_ipl(void** data, size_t* size, address_t parm_addr,
		  address_t initrd_addr, address_t image_addr)
{
	struct boot_tape_ipl_params params;
	void* buffer;

	if (image_addr != (image_addr & PSW_ADDRESS_MASK)) {
		error_reason("Kernel image load address to high (31 bit "
			     "addressing mode)");
		return -1;
	}
	buffer = misc_malloc(DATA_SIZE(tape0));
	if (buffer == NULL)
		return -1;
	/* Prepare params section */
	params.parm_addr = (uint64_t) parm_addr;
	params.initrd_addr = (uint64_t) initrd_addr;
	params.load_psw = (uint64_t) (image_addr | PSW_LOAD);
	/* Initialize buffer */
	memcpy(buffer, DATA_ADDR(tape0), DATA_SIZE(tape0));
	memcpy(VOID_ADD(buffer, BOOT_TAPE_IPL_PARAMS_OFFSET), &params,
	       sizeof(struct boot_tape_ipl_params));
	*data = buffer;
	*size = DATA_SIZE(tape0);
	return 0;
}


int
boot_init_eckd_compatible_dump_stage1(
				struct boot_eckd_compatible_stage1* stage1,
				struct disk_info* info,
				int mvdump_switch)
{
	blocknum_t blocks;
	blocknum_t i;

	memcpy(stage1, DATA_ADDR(eckd1b),
	       sizeof(struct boot_eckd_compatible_stage1));
	if (mvdump_switch)
		blocks = (DATA_SIZE(eckd2mvdump) +
			  info->phy_block_size - 1) / info->phy_block_size;
	else
		blocks = (DATA_SIZE(eckd2dump) +
			  info->phy_block_size - 1) / info->phy_block_size;
	if (blocks > 12) {
		error_reason("Not enough room for ECKD stage 2 dump record "
			     "(try larger block size)");
		return -1;
	}
	/* Fill in block list of stage 2 dump record */
	for (i = 0; i < blocks; i++) {
		stage1->read[i].count = info->phy_block_size;
		stage1->read[i].address_lo = ZIPL_STAGE2_LOAD_ADDRESS +
					     i * info->phy_block_size;
	}
	/* Terminate CCW chain */
	stage1->read[i - 1].flags &= ~ECKD_CCW_FLAG_CC;
	return 0;
}


/* ASCII to EBCDIC conversion table. */
unsigned char ascebc[256] =
{
     0x00, 0x01, 0x02, 0x03, 0x37, 0x2D, 0x2E, 0x2F,
     0x16, 0x05, 0x15, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
     0x10, 0x11, 0x12, 0x13, 0x3C, 0x3D, 0x32, 0x26,
     0x18, 0x19, 0x3F, 0x27, 0x22, 0x1D, 0x1E, 0x1F,
     0x40, 0x5A, 0x7F, 0x7B, 0x5B, 0x6C, 0x50, 0x7D,
     0x4D, 0x5D, 0x5C, 0x4E, 0x6B, 0x60, 0x4B, 0x61,
     0xF0, 0xF1, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6, 0xF7,
     0xF8, 0xF9, 0x7A, 0x5E, 0x4C, 0x7E, 0x6E, 0x6F,
     0x7C, 0xC1, 0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7,
     0xC8, 0xC9, 0xD1, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6,
     0xD7, 0xD8, 0xD9, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6,
     0xE7, 0xE8, 0xE9, 0xBA, 0xE0, 0xBB, 0xB0, 0x6D,
     0x79, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
     0x88, 0x89, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96,
     0x97, 0x98, 0x99, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6,
     0xA7, 0xA8, 0xA9, 0xC0, 0x4F, 0xD0, 0xA1, 0x07,
     0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
     0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
     0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
     0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
     0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
     0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
     0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
     0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
     0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
     0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
     0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
     0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
     0x3F, 0x59, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
     0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
     0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F, 0x3F,
     0x90, 0x3F, 0x3F, 0x3F, 0x3F, 0xEA, 0x3F, 0xFF
};

static void ascii_to_ebcdic(unsigned char* from, unsigned char* to)
{
	for (;from != to; from++)
		*from = ascebc[*from];
}


static int
store_stage2_menu(void* data, size_t size, struct job_data* job)
{
	struct boot_stage2_params* params;
	char* current;
	char* name;
	ssize_t len;
	int flag;
	int timeout;
	int textlen;
	int maxlen;
	int i;

	memset(data, 0, size);
	if (job->id == job_menu) {
		name = "-";
		for (i=0; i < job->data.menu.num; i++)
			if (job->data.menu.entry[i].pos ==
			    job->data.menu.default_pos)
				name = job->data.menu.entry[i].name;
		flag = (job->data.menu.prompt != 0);
		timeout = job->data.menu.timeout;
		/* Be verbose */
		if (verbose) {
			printf("Preparing boot menu\n");
			printf("  Interactive prompt......: %s\n",
			       job->data.menu.prompt ? "enabled" : "disabled");
			if (job->data.menu.timeout == 0)
				printf("  Menu timeout............: "
				       "disabled\n");
			else
				printf("  Menu timeout............: %d "
				       "seconds\n", job->data.menu.timeout);
			printf("  Default configuration...: '%s'\n", name);
		}
	} else {
		name = job->name;
		flag = 0;
		timeout = 0;
	}
	/* Header */
	len = sizeof(struct boot_stage2_params);
	if ((ssize_t) size < len) {
		error_reason("Not enough room for menu data");
		return -1;
	}
	params = (struct boot_stage2_params *) data;
	params->flag = flag;
	params->timeout = timeout;
	current = (char *) (params + 1);
	size -= len;
	/* Banner text */
	len = snprintf(current, size, "zIPL v%s interactive boot menu\n ",
		       RELEASE_STRING) + 1;
	if (len >= (ssize_t) size) {
		error_reason("Not enough room for menu data");
		return -1;
	}
	params->banner = (uint16_t) ((unsigned long) current -
				     (unsigned long) data);
	current += len;
	size -= len;
	/* Default config text */
	if (name != NULL)
		len = snprintf(current, size, " 0. default (%s)", name) + 1;
	else
		len = snprintf(current, size, " 0. default") + 1;
	if (len >= (ssize_t ) size) {
		error_reason("Not enough room for menu data");
		return -1;
	}
	params->config[0] = (uint16_t) ((unsigned long) current -
					(unsigned long) data);
	current += len;
	size -= len;
	/* Skip rest if job is not an actual menu */
	if (job->id != job_menu) {
		/* Convert to EBCDIC */
		ascii_to_ebcdic((unsigned char *) (params + 1),
				(unsigned char *) current);
		return 0;
	}
	/* Config texts */
	textlen = 0;
	maxlen = 0;
	for (i = 0; i < job->data.menu.num; i++) {
		len = strlen(job->data.menu.entry[i].name);
		if (len > maxlen)
			maxlen = len;
		textlen += len + 1;
	}
	maxlen += 5; /* '%2d. \0' */
	if ((size_t) textlen > size) {
		/* Menu text won't fit, need to truncate */
		maxlen = size / job->data.menu.num;
		if (maxlen < 6) {
			error_reason("Not enough room for menu data (try "
				     "shorter config names)");
			return -1;
		}
		fprintf(stderr, "Warning: Menu text too large, truncating "
			"names at %d characters!\n", maxlen - 5);
	}
	for (i = 0; i < job->data.menu.num; i++) {
		len = snprintf(current, maxlen, "%2d. %s",
			       job->data.menu.entry[i].pos,
			       job->data.menu.entry[i].name) + 1;
		if (len > maxlen)
			len = maxlen;
		params->config[job->data.menu.entry[i].pos] = (uint16_t)
			((unsigned long) current - (unsigned long) data);
		current += len;
	}
	/* Convert to EBCDIC */
	ascii_to_ebcdic((unsigned char *) (params + 1),
			(unsigned char *) current);
	return 0;
}



int
boot_get_fba_stage2(void** data, size_t* size, struct job_data* job)
{
	void* buffer;
	int rc;

	buffer = misc_malloc(stage2_max_size);
	if (buffer == NULL)
		return -1;
	memcpy(buffer, DATA_ADDR(fba2), DATA_SIZE(fba2));
	rc = store_stage2_menu(VOID_ADD(buffer, DATA_SIZE(fba2)),
			       stage2_max_size - DATA_SIZE(fba2),
			       job);
	if (rc) {
		free(buffer);
		return rc;
	}
	*data = buffer;
	*size = stage2_max_size;
	return 0;
}


int
boot_get_eckd_stage2(void** data, size_t* size, struct job_data* job)
{
	void* buffer;
	int rc;

	buffer = misc_malloc(stage2_max_size);
	if (buffer == NULL)
		return -1;
	memcpy(buffer, DATA_ADDR(eckd2), DATA_SIZE(eckd2));
	rc = store_stage2_menu(VOID_ADD(buffer, DATA_SIZE(eckd2)),
			       stage2_max_size - DATA_SIZE(eckd2),
			       job);
	if (rc) {
		free(buffer);
		return rc;
	}
	*data = buffer;
	*size = stage2_max_size;
	return 0;
}


int
boot_get_virtio_stage2(void** data, size_t* size, struct job_data* job)
{
	void* buffer;
	int rc;

	stage2_max_size = (16 * 512);
	buffer = misc_malloc(stage2_max_size);
	if (buffer == NULL)
		return -1;
	memcpy(buffer, DATA_ADDR(virtio2), DATA_SIZE(virtio2));
	rc = store_stage2_menu(VOID_ADD(buffer, DATA_SIZE(virtio2)),
			       stage2_max_size - DATA_SIZE(virtio2),
			       job);
	if (rc) {
		free(buffer);
		return rc;
	}
	*data = buffer;
	*size = stage2_max_size;
	return 0;
}


int
boot_get_tape_dump(void** data, size_t* size, uint64_t mem)
{
	void* buffer;

	buffer = misc_malloc(DATA_SIZE(tapedump));
	if (buffer == NULL)
		return -1;
	memcpy(buffer, DATA_ADDR(tapedump), DATA_SIZE(tapedump));
	/* Write mem size to end of dump record */
	memcpy(VOID_ADD(buffer, DATA_SIZE(tapedump) - sizeof(mem)), &mem,
	       sizeof(mem));
	*data = buffer;
	*size = DATA_SIZE(tapedump);
	return 0;
}


int
boot_get_eckd_dump_stage2(void** data, size_t* size, uint64_t mem)
{
	void* buffer;

	buffer = misc_malloc(DATA_SIZE(eckd2dump));
	if (buffer == NULL)
		return -1;
	memcpy(buffer, DATA_ADDR(eckd2dump), DATA_SIZE(eckd2dump));
	/* Write mem size to end of dump record */
	memcpy(VOID_ADD(buffer, DATA_SIZE(eckd2dump) - sizeof(mem)),
	       &mem, sizeof(mem));
	*data = buffer;
	*size = DATA_SIZE(eckd2dump);
	return 0;
}

int
boot_get_eckd_mvdump_stage2(void** data, size_t* size, uint64_t mem,
			    uint8_t force, struct mvdump_parm_table parm)
{
	void* buffer;

	buffer = misc_malloc(DATA_SIZE(eckd2mvdump));
	if (buffer == NULL)
		return -1;
	memcpy(buffer, DATA_ADDR(eckd2mvdump), DATA_SIZE(eckd2mvdump));
	/* Write mem size and force indicator (as specified by zipl -M)
	 * to end of dump record, right before 512-byte parameter table */
	memcpy(VOID_ADD(buffer, DATA_SIZE(eckd2mvdump) - sizeof(mem) -
			sizeof(struct mvdump_parm_table)), &mem, sizeof(mem));
	memcpy(VOID_ADD(buffer, DATA_SIZE(eckd2mvdump) - sizeof(mem) -
			sizeof(force) - sizeof(struct mvdump_parm_table)),
	       &force, sizeof(force));
	memcpy(VOID_ADD(buffer, DATA_SIZE(eckd2mvdump) -
			sizeof(struct mvdump_parm_table)), &parm,
	       sizeof(struct mvdump_parm_table));
	*data = buffer;
	*size = DATA_SIZE(eckd2mvdump);
	return 0;
}


int
boot_get_fba_dump_stage2(void** data, size_t* size, uint64_t mem)
{
	void* buffer;

	buffer = misc_malloc(DATA_SIZE(fba2dump));
	if (buffer == NULL)
		return -1;
	memcpy(buffer, DATA_ADDR(fba2dump), DATA_SIZE(fba2dump));
	/* Write mem size to end of dump record */
	memcpy(VOID_ADD(buffer, DATA_SIZE(fba2dump) - sizeof(mem)),
	       &mem, sizeof(mem));
	*data = buffer;
	*size = DATA_SIZE(fba2dump);
	return 0;
}
