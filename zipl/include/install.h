/*
 * s390-tools/zipl/include/install.h
 *   Functions handling the installation of the boot loader code onto disk.
 *
 * Copyright IBM Corp. 2001, 2009.
 *
 * Author(s): Carsten Otte <cotte@de.ibm.com>
 *            Peter Oberparleiter <Peter.Oberparleiter@de.ibm.com>
 */

#ifndef INSTALL_H
#define INSTALL_H

#include "zipl.h"

#include "disk.h"
#include "job.h"


int install_bootloader(const char* device, disk_blockptr_t* program_table,
		       disk_blockptr_t* stage2_data, blocknum_t stage2_count,
		       struct disk_info* info, struct job_data* job);
int install_tapeloader(const char* device, const char* image,
		       const char* parmline, const char* ramdisk,
		       address_t image_addr, address_t parm_addr,
		       address_t initrd_addr);
int install_dump(const char* device, struct job_target_data* target,
		 uint64_t mem);
int install_mvdump(char* const device[], struct job_target_data* target,
		   int device_count, uint64_t mem, uint8_t force);

#endif /* INSTALL_H */
