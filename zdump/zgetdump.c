/*
 *  zgetdump
 *    Description: The zgetdump tool takes as input the dump device
 *		 and writes its contents to standard output,
 *		 which you can redirect to a specific file.
 *
 *    Copyright IBM Corp. 2001, 2006.
 *    Author(s): Despina Papadopoulou
 *               Frank Munzert <munzert@de.ibm.com>
 */

#include "zgetdump.h"
#include "zt_common.h"
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <fcntl.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <getopt.h>
#include <limits.h>
#include <dirent.h>
#include <errno.h>
#include <sys/mtio.h>

/* from linux/fs.h */
#define BLKSSZGET		_IO(0x12,104)
#define BLKFLSBUF		_IO(0x12,97)

#define HEADER_SIZE 4096
#define BLOCK_SIZE 32768
#define MVDUMPER_SIZE 4096
#define PARTN_MASK ((1 << 2) - 1)
#define MAGIC_BLOCK_OFFSET_ECKD 3
#define MAGIC_OFFSET_FBA -0x1000
#define HEXINSTR "\x0d\x10\x47\xf0"      /* BASR + 1st halfword of BC    */
#define ARCH_S390  1
#define ARCH_S390X 2
#define VERSION_NO_DUMP_DEVICE -1

#define SYSFS_BUSDIR "/sys/bus/ccw/devices"

#if defined(__s390x__)
	#define FMT64 "l"
#else
	#define FMT64 "ll"
#endif

/*  definitions  */

char *help_text =
"The zgetdump tool takes as input the dump device and writes its contents\n"\
"to standard output, which you can redirect to a specific file.\n"\
"zgetdump can also check, whether a DASD device contains a valid dumper.\n\n"\
"Usage:\n"\
"Copy dump from <dumpdevice> to stdout:\n"\
"       > zgetdump <dumpdevice>\n"\
"Print dump header and check if dump is valid - for single tape or DASD:\n"\
"       > zgetdump [-i | --info] <dumpdevice>\n"\
"Print dump header and check if dump is valid - for all volumes of a\n"
"multi-volume tape dump:\n"\
"       > zgetdump [-i | --info] [-a | --all] <dumpdevice>\n"\
"Check dump device:\n"\
"       > zgetdump [-d | --device] <dasd_device>\n"\
"Print version info:\n"\
"       > zgetdump [-v | --version]\n"\
"Print this text:\n"\
"       > zgetdump [-h | --help]\n\n"\
"Examples for single-volume DASD:\n"\
"> zgetdump -d /dev/dasdc\n"\
"> zgetdump -i /dev/dasdc1\n"\
"> zgetdump /dev/dasdc1 > dump_file\n";

char *usage_note =
"Usage:\n"\
"> zgetdump <dumpdevice>\n"\
"> zgetdump -i <dumpdevice>\n"\
"> zgetdump -i -a <dumpdevice>\n"\
"> zgetdump -d <device>\n"\
"More info:\n"\
"> zgetdump -h\n";

/* Version info */
static const char version_text[] = "zgetdump: version "RELEASE_STRING;

/* Copyright notice */
static const char copyright_notice[] = "Copyright IBM Corp. 2001, 2008";

/* global variables */

s390_dump_header_t  header;
s390_dump_end_marker_t  end_marker;
char read_buffer[BLOCK_SIZE];
struct timeval h_time_begin, h_time_end;

int  option_a_set;
int  option_i_set;
int  option_d_set;
char dump_device[PATH_MAX];

/* end of definitions */

/* Use uname to check whether we run s390x kernel */
int check_kernel_mode()
{
	struct utsname uname_struct;
	if (uname(&uname_struct)) {
		fprintf(stderr, "Unable to get name and information about "
			"current kernel. \n");
		perror("");
		return 1;
	}
	if (strncmp(uname_struct.machine, "s390x", 5) == 0) {
		fprintf(stderr, "=========================================="
			"=======\n");
		fprintf(stderr, "WARNING: You are running an s390x (ESAME) "
			"kernel.\n");
		fprintf(stderr, "         Your dump tool however is s390 "
			"(ESA).\n");
		fprintf(stderr, "=========================================="
			"=======\n");
	}
	return 0;
}

/* Read dump tool from DASD device */
int read_dumper(int fd, int32_t offset, struct dump_tool *buffer, int whence)
{
	if (lseek(fd, offset, whence) == -1) {
		perror("Cannot seek on device");
		return 1;
	}
	if (read(fd, buffer, sizeof(struct dump_tool)) !=
	    sizeof(struct dump_tool)) {
		perror("Cannot read dump tool from device");
		return 1;
	}
	return 0;
}

/* Use stat to check whether user provided input is a block device or a
 * partition */
enum devnode_type check_device(char *device, int print)
{
	struct stat stat_struct;

	if (stat(device, &stat_struct)) {
		fprintf(stderr, "Unable to get device status for "
			"'%s'. \n", device);
		perror("");
		return IS_NOBLOCK;
	}
	if (!(S_ISBLK(stat_struct.st_mode))) {
		fprintf(stderr, "'%s' is not a block device. \n", dump_device);
		return IS_NOBLOCK;
	}
	if (minor(stat_struct.st_rdev) & PARTN_MASK) {
		if (print)
			fprintf(stderr, "Partition '%s' (%d/%d) specified where"
				" device is required.\n", dump_device,
				(unsigned short) major(stat_struct.st_rdev),
				(unsigned short) minor(stat_struct.st_rdev));
		return IS_PARTITION;
	}
	return IS_DEVICE;
}

/* Allocate SIZE bytes of memory. Upon success, return pointer to memory.
 * Return NULL otherwise. */
void *misc_malloc(size_t size)
{
	void* result;

	result = malloc(size);
	if (result == NULL) {
		fprintf(stderr, "Could not allocate %lld bytes of memory",
			(unsigned long long) size);
	}
	return result;
}

char* misc_make_path(char* dirname, char* filename)
{
	char* result;
	size_t len;

	len = strlen(dirname) + strlen(filename) + 2;
	result = (char *) misc_malloc(len);
	if (result == NULL)
		return NULL;
	sprintf(result, "%s/%s", dirname, filename);
	return result;
}

#define TEMP_DEV_MAX_RETRIES	1000

/* Make temporary device node for input device identified by its dev_t */
int make_temp_devnode(dev_t dev, char** device_node)
{
	char* result;
	char* pathname[] = { getenv("TMPDIR"), "/tmp",
			     getenv("HOME"), "." , "/"};
	char filename[] = "zgetdump0000";
	mode_t mode;
	unsigned int path;
	int retry;
	int rc;
	int fd;

	mode = S_IFBLK | S_IRWXU;
	/* Try several locations as directory for the temporary device
	 * node. */
	for (path=0; path < sizeof(pathname) / sizeof(pathname[0]); path++) {
		if (pathname[path] == NULL)
			continue;
		for (retry=0; retry < TEMP_DEV_MAX_RETRIES; retry++) {
			sprintf(filename, "zgetdump%04d", retry);
			result = misc_make_path(pathname[path], filename);
			if (result == NULL)
				return 1;
			rc = mknod(result, mode, dev);
			if (rc == 0) {
				/* Need this test to cover 'nodev'-mounted
				 * filesystems. */
				fd = open(result, O_RDWR);
				if (fd != -1) {
					close(fd);
					*device_node = result;
					return 0;
				}
				remove(result);
				retry = TEMP_DEV_MAX_RETRIES;
			} else if (errno != EEXIST)
				retry = TEMP_DEV_MAX_RETRIES;
			free(result);
		}
	}
	fprintf(stderr, "Unable to create temporary device node: %s",
		strerror(errno));
	return 1;
}

/* Delete temporary device node and free memory allocated for device name. */
void free_temp_devnode(char* device_node)
{
	if (remove(device_node)) {
		fprintf(stderr, "Warning: Could not remove "
				"temporary file %s: %s",
				device_node, strerror(errno));
	}
	free(device_node);
}


int open_block_device(char *device)
{
	int fd;

	if (check_device(device, 1) != IS_DEVICE)
		return -1;
	fd = open(device, O_RDONLY);
	if (fd == -1) {
		fprintf(stderr, "Cannot open device '%s'. \n", device);
		perror("");
	}
	return fd;
}

/* Check sysfs, whether a device specified by its bus id is defined and online.
 * Find out the corresponding dev_t */
enum device_status get_device_from_busid(char* bus_id, dev_t *device)
{
	char dev_file[PATH_MAX];
	char temp_file[PATH_MAX];
	char buffer[10];
	struct dirent *direntp;
	int fd, minor, major;
	DIR *fd1;

	fd1 = opendir(SYSFS_BUSDIR);
	if (!fd1) {
		fprintf(stderr, "Could not open %s (err = %i).\n",
			SYSFS_BUSDIR, errno);
		exit(1);	/* sysfs info not available		*/
	}
	closedir(fd1);
	snprintf(dev_file, PATH_MAX, "%s/%s", SYSFS_BUSDIR, bus_id);
	fd1 = opendir(dev_file);
	if (!fd1)
		return UNDEFINED; /* device with devno does not exist	*/
	snprintf(temp_file, PATH_MAX, "%s/online", dev_file);
	fd = open(temp_file, O_RDONLY);
	if (read(fd, buffer, 1) == -1) {
		perror("Could not read online attribute.");
		exit(1);
	}
	close(fd);
	if (buffer[0] != '1')
		return OFFLINE;   /* device with devno is not online	*/
	while ((direntp = readdir(fd1)))
		if (strncmp(direntp->d_name, "block:", 6) == 0)
			break;
	closedir(fd1);
	if (direntp == NULL) {
		snprintf(dev_file, PATH_MAX, "%s/%s/block", SYSFS_BUSDIR,
			 bus_id);
		fd1 = opendir(dev_file);
		if (!fd1) {
			fprintf(stderr, "Could not open %s (err = %i).\n",
				dev_file, errno);
			exit(1);
		}
		while ((direntp = readdir(fd1)))
			if (strncmp(direntp->d_name, "dasd", 4) == 0)
				break;
		closedir(fd1);
		if (direntp == NULL) {
			fprintf(stderr, "Problem with contents of %s.\n",
				dev_file);
			exit(1);
		}
	}
	snprintf(temp_file, PATH_MAX, "%s/%s/dev", dev_file, direntp->d_name);
	fd = open(temp_file, O_RDONLY);
	if (read(fd, buffer, sizeof(buffer)) == -1) {
		perror("Could not read dev file.");
		exit(1);
	}
	close(fd);
	if (sscanf(buffer, "%i:%i", &major, &minor) != 2) {
		fprintf(stderr, "Malformed content of %s: %s\n",
			temp_file, buffer);
		exit(1);
	}
	*device = makedev(major, minor);
	return ONLINE;
}

/* Read dump tool, multi-volume dump parameter table, and dump header from the
 * input dump volume. Check input dump volume for
 * - identical dump parameter table (that is it belongs to the same dump set)
 * - valid magic number in the dump tool
 * - valid dump signature in the dump header
 * and set the volume's signature accordingly */
int get_mvdump_volume_info(struct disk_info *vol, uint32_t vol_nr, off_t offset,
			   struct mvdump_parm_table *table)
{
	int fd, rc;
	ssize_t n_read;
	char* temp_devnode;
	struct dump_tool dumper;
	struct mvdump_parm_table vol_table;

	vol->signature = INVALID;
	rc = make_temp_devnode(vol->device, &temp_devnode);
	if (rc)
		return 1;
	fd = open_block_device(temp_devnode);
	if (fd == -1) {
		free_temp_devnode(temp_devnode);
		return 1;
	}
	/* We read partition data via the device node. If another process
	 * has changed partition data via the partition node, the corresponding
	 * device node might still have old data in its buffers. Flush buffers
	 * to keep things in sync */
	if (ioctl(fd, BLKFLSBUF, 0)) {
		perror("BLKFLSBUF failed");
		goto out;
	}
	if (read_dumper(fd, offset, &dumper, SEEK_SET))
		goto out;
	if (lseek(fd, offset + MVDUMPER_SIZE, SEEK_SET) !=
	    offset + MVDUMPER_SIZE) {
		perror("Cannot seek on device");
		goto out;
	}
	n_read = read(fd, &vol_table, sizeof(vol_table));
	if (n_read == -1) {
		perror("Cannot read multi-volume dump table");
		goto out;
	}
	/* Check whether dump table on user specified dump device is
	 * identical to the one found on this device */
	if (memcmp(&vol_table, table, sizeof(vol_table))) {
		printf("ERROR: Orphaned multi-volume dump device '%s'\n",
		       dump_device);
		goto out;
	}
	if (lseek(fd, vol->start_offset, SEEK_SET) != vol->start_offset) {
		perror("Cannot seek on device");
		goto out;
	}
	n_read = read(fd, &header, HEADER_SIZE);
	if (n_read == -1) {
		perror("Cannot read dump header");
		goto out;
	}
	free_temp_devnode(temp_devnode);
	close(fd);
	if ((header.dh_mvdump_signature == DUMP_MAGIC_S390) &&
	    (strncmp(dumper.magic, "ZMULT64", 7) == 0)) {
		vol->signature = VALID;
		if ((header.dh_volnr == vol_nr) && (header.dh_memory_size != 0))
			vol->signature = ACTIVE;
	}
	return 0;
out:
	free_temp_devnode(temp_devnode);
	close(fd);
	return 1;
}

/* Read multi-volume dump parameter table from dump device and fill in the
 * fields of the disk_info array */
int get_mvdump_info(int fd, int block_size, int *count,
		    struct disk_info vol[])
{
	int i, rc = 0;
	off_t offset;
	ssize_t n_read;
	struct mvdump_parm_table table;

	offset = MAGIC_BLOCK_OFFSET_ECKD * block_size + MVDUMPER_SIZE;
	if (lseek(fd, offset, SEEK_SET) != offset) {
		fprintf(stderr, "Cannot seek on device '%s'.\n",
			dump_device);
		perror("");
		return 1;
	}
	n_read = read(fd, &table, sizeof(table));
	if (n_read == -1) {
		perror("Cannot read multi-volume dump table");
		return 1;
	}
	*count = table.num_param;
	for (i = 0; i < table.num_param; i++) {
		sprintf(vol[i].bus_id, "0.0.%04x", table.param[i].devno);
		vol[i].start_offset = table.param[i].start_blk;
		vol[i].start_offset *= table.param[i].blocksize << 8;
		vol[i].part_size = (table.param[i].end_blk -
				    table.param[i].start_blk + 1);
		vol[i].part_size *= table.param[i].blocksize << 8;
		vol[i].status = get_device_from_busid(vol[i].bus_id,
						      &vol[i].device);
		if (vol[i].status == ONLINE) {
			offset = MAGIC_BLOCK_OFFSET_ECKD *
				table.param[i].blocksize << 8;
			rc = get_mvdump_volume_info(&vol[i], i, offset,
						    &table);
			if (rc)
				return rc;
		}
	}
	return 0;
}

/* Print dump size limit as specified in zipl -d or zipm -M */
void print_size_limit_info(uint64_t memory)
{
	fprintf(stderr, "Dump size limit: ");
	if (memory == (uint64_t) -1)
		fprintf(stderr, "none\n");
	else
		fprintf(stderr, "%lldMB\n", (unsigned long long) memory /
			(1024LL * 1024LL));
}

/* Print multi-volume dump device information for --device option */
void print_mvdump_info(int version, int count, struct disk_info vol[],
		       uint64_t memory, int force)
{
	int i;

	fprintf(stderr, "'%s' is part of Version %i multi-volume dump,\n"
		"which is spread along the following DASD volumes:\n",
		dump_device, version);
	for (i = 0; i < count; i++) {
		switch(vol[i].status) {
		case UNDEFINED:
			fprintf(stderr, "%s (not defined)\n", vol[i].bus_id);
			break;
		case OFFLINE:
			fprintf(stderr, "%s (offline)\n", vol[i].bus_id);
			break;
		case ONLINE:
			fprintf(stderr, "%s (online, ", vol[i].bus_id);
			if (vol[i].signature == INVALID)
				fprintf(stderr, "invalid)\n");
			else
				fprintf(stderr, "valid)\n");
			break;
		}
	}
	print_size_limit_info(memory);
	fprintf(stderr, "Force option specified: ");
	if (force)
		fprintf(stderr, "yes\n");
	else
		fprintf(stderr, "no\n");
}

/* Print single-volume dump device information for --device option */
int print_dump_info(int version, int dumper_arch, uint64_t memory)
{
	int rc = 0;

	if (version > 0) {
		if (dumper_arch == ARCH_S390) {
			fprintf(stderr, "'%s' is Version %i s390 (ESA) "
				"dump device.\n", dump_device, version);
			if (check_kernel_mode())
				rc = 1;
		} else
			fprintf(stderr, "'%s' is Version %i s390x (ESAME) "
				"dump device.\n", dump_device, version);
	} else
		fprintf(stderr, "'%s' is Version 0 dump device. \n",
			dump_device);
	print_size_limit_info(memory);
	return rc;
}

/* Read dump tool on FBA disk and check its magic number */
int check_dump_tool_fba(int fd, int *version, int *arch, uint64_t *memory)
{
	struct dump_tool dumper;

	if (read_dumper(fd, MAGIC_OFFSET_FBA, &dumper, SEEK_END))
		return 1;
	*memory = dumper.mem;
	if (strncmp(dumper.magic, "ZDFBA31", 7) == 0) {
		*version = dumper.version;
		*arch = ARCH_S390;
	} else if (strncmp(dumper.magic, "ZDFBA64", 7) == 0) {
		*version = dumper.version;
		*arch = ARCH_S390X;
	} else if ((memcmp(dumper.magic, HEXINSTR, 4) == 0) &&
		   (dumper.code[0] == '\x0d') && (dumper.code[1] == '\xd0'))
		/* We found basr r13,0 (old dumper) */
		*version = 0;
	else
		*version = VERSION_NO_DUMP_DEVICE;
	return 0;
}

/* Read dump tool on ECKD disk and check its magic number */
int check_dump_tool_eckd(int fd, int *version, int *arch, int *dasd_mv_flag,
			  int *block_size, int *force_specified,
			  uint64_t *memory)
{
	struct dump_tool dumper;

	if (ioctl(fd, BLKSSZGET, block_size)) {
		fprintf(stderr, "Cannot get blocksize of device %s.\n",
			dump_device);
		perror("");
		return 1;
	}
	if (read_dumper(fd, MAGIC_BLOCK_OFFSET_ECKD * *block_size, &dumper,
		       SEEK_SET))
		return 1;
	*memory = dumper.mem;
	if (strncmp(dumper.magic, "ZECKD31", 7) == 0) {
		*version = dumper.version;
		*arch = ARCH_S390;
	} else if (strncmp(dumper.magic, "ZECKD64", 7) == 0) {
		*version = dumper.version;
		*arch = ARCH_S390X;
	} else if (strncmp(dumper.magic, "ZMULT64", 7) == 0) {
		*version = dumper.version;
		*arch = ARCH_S390X;
		*dasd_mv_flag = 1;
		*force_specified = dumper.force;
	} else if ((memcmp(dumper.magic, HEXINSTR, 4) == 0) &&
		   (dumper.code[0] == '\x0d') && (dumper.code[1] == '\xd0'))
		/* We found basr r13,0 (old dumper) */
		*version = 0;
	else
		*version = VERSION_NO_DUMP_DEVICE;
	return 0;
}

void s390_tod_to_timeval(uint64_t todval, struct timeval *xtime)
{
    /* adjust todclock to 1970 */
    todval -= 0x8126d60e46000000LL - (0x3c26700LL * 1000000 * 4096);

    todval >>= 12;
    xtime->tv_sec  = todval / 1000000;
    xtime->tv_usec = todval % 1000000;
}


int open_dump(char *pathname)
{
	int fd;

	fd = open(pathname, O_RDONLY);
	if (fd == -1) {
		perror("Cannot open dump device");
		exit(1);
	} else
		fprintf(stderr, "Dump device: %s\n", pathname);
	return fd;
}


/* check if device is dasd or tape */
enum dump_type dev_type(int fd)
{
	struct mtget mymtget;

	if (ioctl(fd, MTIOCGET, &mymtget) == -1)
		return IS_DASD;
	else
		return IS_TAPE;
}

/* print lkcd header information */
void print_lkcd_header(int fd)
{
	dump_header_4_1_t dump_header;

	lseek(fd, 0, SEEK_SET);
	if (read(fd, &dump_header, sizeof(dump_header)) == -1) {
		perror("Could not read dump header.");
		exit(1);
	}
	fprintf(stderr, "\nThis is a lkcd dump:\n\n");
	fprintf(stderr,
		"Memory start   : 0x%"FMT64"x\n", dump_header.dh_memory_start);
	fprintf(stderr,
		"Memory end     : 0x%"FMT64"x\n", dump_header.dh_memory_end);
	fprintf(stderr,
		"Physical memory: %"FMT64"d\n", dump_header.dh_memory_size);
	fprintf(stderr,
		"Panic string   : %s\n", dump_header.dh_panic_string);
	fprintf(stderr,
		"Number of pages: %d\n", dump_header.dh_num_dump_pages);
	fprintf(stderr,
		"Page size      : %d\n", dump_header.dh_dump_page_size);
	fprintf(stderr,
		"Magic number   : 0x%"FMT64"x\n", dump_header.dh_magic_number);
	fprintf(stderr,
		"Version number : %d\n", dump_header.dh_version);
}

void print_s390_header(enum dump_type d_type)
{
	s390_tod_to_timeval(header.dh_tod, &h_time_begin);

/*	as from version 2 of the dump tools	*/
/*	volume numbers are used			*/

	if ((d_type == IS_TAPE) && (header.dh_version >= 2)) {
		fprintf(stderr, "\nTape Volume %i", header.dh_volnr);
		if (header.dh_volnr != 0)
			fprintf(stderr, " of a multi volume dump.\n");
		else
			fprintf(stderr, "\n");
	}

/*	don't print header			*/
/*	for all subsequent tapes/disks		*/
/*	of a multi-volume tape/disk dump	*/

	if ((d_type == IS_DASD) || (header.dh_volnr == 0)) {
		if (header.dh_magic_number != DUMP_MAGIC_S390) {
			fprintf(stderr, "===================================="
				"===============\n");
			fprintf(stderr, "WARNING: This does not look like a "
				"valid s390 dump!\n");
			fprintf(stderr, "===================================="
				"===============\n");
		}
		fprintf(stderr, "\n>>>  Dump header information  <<<\n");
		fprintf(stderr, "Dump created on: %s\n",
			ctime(&h_time_begin.tv_sec));
		fprintf(stderr, "Magic number:\t 0x%"FMT64"x\n",
			header.dh_magic_number);
		fprintf(stderr, "Version number:\t %d\n", header.dh_version);
		fprintf(stderr, "Header size:\t %d\n", header.dh_header_size);
		fprintf(stderr, "Page size:\t %d\n", header.dh_page_size);
		fprintf(stderr, "Dumped memory:\t %"FMT64"d\n",
			header.dh_memory_size);
		fprintf(stderr, "Dumped pages:\t %u\n", header.dh_num_pages);
		if (header.dh_version >= 3) {
			fprintf(stderr, "Real memory:\t %"FMT64"d\n",
				header.dh_real_memory_size);
		}
		fprintf(stderr, "cpu id:\t\t 0x%"FMT64"x\n", header.dh_cpu_id);
		if (header.dh_version >= 2) {
			switch (header.dh_arch) {
			case 1: fprintf(stderr, "System Arch:\t s390 (ESA)\n");
				break;
			case 2: fprintf(stderr,
					"System Arch:\t s390x (ESAME)\n");
				break;
			default:
				fprintf(stderr, "System Arch:\t <unknown>\n");
				break;
			}
			switch (header.dh_build_arch) {
			case 1: fprintf(stderr, "Build Arch:\t s390 (ESA)\n");
				break;
			case 2: fprintf(stderr,
					"Build Arch:\t s390x (ESAME)\n");
				break;
			default:
				fprintf(stderr, "Build Arch:\t <unknown>\n");
				break;
			}
		}
		fprintf(stderr, ">>>  End of Dump header  <<<\n\n");
	}
}

/* print header information */
void get_header(int fd)
{
	ssize_t n_read;

	n_read = read(fd, &header, HEADER_SIZE);
	if (n_read == -1) {
		perror("Cannot read dump header");
		close(fd);
		exit(1);
	}
}

/* copy header to stdout */
void write_header()
{
	ssize_t rc;

	memcpy(read_buffer, &header, sizeof(header));
	rc = write(STDOUT_FILENO, read_buffer, header.dh_header_size);
	if (rc == -1) {
		perror("\nwrite failed");
		exit(1);
	}
	if (rc < header.dh_header_size) {
		fprintf(stderr, "\nwrite failed: No space left on device\n");
		exit(1);
	}
}

/* copy partition containing multi-volume dump data to stdout */
int mvdump_copy(int fd, uint64_t partsize, uint64_t *totalsize)
{
	ssize_t n_read, n_written;
	uint64_t part_offset;
	int done = 0;
	size_t count;

	part_offset = HEADER_SIZE;
	do {
		count = MIN(header.dh_memory_size - *totalsize, BLOCK_SIZE);
		if (count < BLOCK_SIZE)
			done = 1;
		if (partsize - part_offset < count) {
			count = partsize - part_offset;
			done = 1;
		}
		n_read = read(fd, read_buffer, count);
		if (n_read == -1) {
			perror("\nread failed");
			return 1;
		}
		n_read = (n_read >> 12) << 12;
		n_written = write(STDOUT_FILENO, read_buffer, n_read);
		if (n_written == -1) {
			perror("\nwrite failed");
			return 1;
		}
		if (n_written < n_read) {
			fprintf(stderr, "\nwrite failed: "
				"No space left on device\n");
			return 1;
		}
		part_offset += n_written;
		*totalsize += n_written;
		if (part_offset % (header.dh_memory_size / 32) == HEADER_SIZE)
			fprintf(stderr, ".");
	} while (!done);
	fprintf(stderr, "\n");
	return 0;
}

/* copy the dump to stdout */
int get_dump(int fd, int d_type)
{
	int ret, bsr;
	ssize_t n_read, n_written;
	struct mtop mymtop;
	uint64_t i;

	ret = 0;
	if (d_type == IS_DASD) {
		i = 0;
		do {
			n_read = read(fd, read_buffer, BLOCK_SIZE);
			n_written = write(STDOUT_FILENO, read_buffer, n_read);
			if (n_written == -1) {
				perror("\nwrite failed");
				exit(1);
			}
			if (n_written < n_read) {
				fprintf(stderr, "\nwrite failed: "
					"No space left on device\n");
				exit(1);
			}
			i += n_read;
			if (i % (header.dh_memory_size / 32) == 0)
				fprintf(stderr, ".");
		} while (i < header.dh_memory_size && n_read != 0
			 && n_written >= 0);
	} else if (d_type == IS_TAPE) {
	/* write to stdout while not ENDOFVOL or DUMP_END		*/
		if (header.dh_volnr != 0)
			fprintf(stderr, "Reading dump content ");
		for (i = 0; i < (header.dh_memory_size/BLOCK_SIZE); i++) {
			n_read = read(fd, read_buffer, BLOCK_SIZE);
			if (i % ((header.dh_memory_size/BLOCK_SIZE) / 32) == 0)
				fprintf(stderr, ".");
			if (strncmp(read_buffer, "ENDOFVOL", 8) == 0) {
				fprintf(stderr, "\nEnd of Volume reached.\n");
				ret = 1;
				break;
			} else if (strncmp(read_buffer, "DUMP_END", 8) == 0) {
				ret = 2;
				break;
			} else {
				n_written = write(STDOUT_FILENO, read_buffer,
						  n_read);
				if (n_written == -1) {
					perror("\nwrite failed");
					exit(1);
				}
				if (n_written < n_read) {
					fprintf(stderr, "\nwrite failed: "
						"No space left on device\n");
					exit(1);
				}
			}
		}
		if (ret == 2) {
			/* we go back a record, so dump_end_times gets called */
			mymtop.mt_count = 1;
			mymtop.mt_op = MTBSR;
			bsr = ioctl(fd, MTIOCTOP, &mymtop);
			if (bsr != 0) {
				fprintf(stderr,
					"Tape operation MTBSR failed.\n");
				exit(1);
			}
		}
	}
	return ret;
}

/*	check for DUMP_END and see		*/
/*	if dump ended after it started (!!!)	*/
int dump_end_times(int fd)
{
	int ret;

	if (read(fd, &end_marker, sizeof(end_marker)) == -1) {
		perror("Could not read end marker.");
		exit(1);
	}
	s390_tod_to_timeval(end_marker.end_time, &h_time_end);
	if ((strncmp(end_marker.end_string, "DUMP_END", 8) == 0) &&
	    ((h_time_end.tv_sec - h_time_begin.tv_sec) >= 0)) {
		fprintf(stderr, "\nDump ended on:\t %s\n",
			ctime(&h_time_end.tv_sec));
		ret = 0;
	} else
		ret = -1;
	return ret;
}

int check_and_write_end_marker(int fd)
{
	if (dump_end_times(fd) == 0) {
		ssize_t rc;
		rc = write(STDOUT_FILENO, &end_marker,
			   sizeof(end_marker));
		if (rc == -1) {
			perror("\nwrite failed");
			return 1;
		}
		if (rc < (ssize_t) sizeof(end_marker)) {
			fprintf(stderr, "\nwrite failed: "
				"No space left on device\n");
			return 1;
		}
		fprintf(stderr, "\nDump End Marker found: "
			"this dump is valid.\n");
		return 0;
	} else {
		fprintf(stderr, "\nThis dump is NOT valid.\n");
		return 1;
	}
}

/*	if a tape is part of the dump (not the last)	*/
/*	it should have and ENDOFVOL marker		*/
int vol_end(void)
{
	int ret;

	ret = strncmp(end_marker.end_string, "ENDOFVOL", 8);
	return ret;
}

/*	position the tape in front of an end marker	*/
/*	with FSFM and BSR				*/
void tape_forwards(int fd)
{
	int ret;
	struct mtop mymtop;

	mymtop.mt_count = 1;
	mymtop.mt_op = MTFSFM;
	ret = ioctl(fd, MTIOCTOP, &mymtop);
	if (ret != 0) {
		fprintf(stderr, "Tape operation FSFM failed.\n");
		exit(1);
	}

	mymtop.mt_count = 1;
	mymtop.mt_op = MTBSR;
	ret = ioctl(fd, MTIOCTOP, &mymtop);
	if (ret != 0) {
		fprintf(stderr, "Tape operation BSR failed.\n");
		exit(1);
	}
}

/*	put current tape offline	*/
/*	load & rewind next tape		*/
void load_next(int fd)
{
	int ret;
	struct mtop mymtop;

	mymtop.mt_count = 1;
	mymtop.mt_op = MTOFFL;
	ret = ioctl(fd, MTIOCTOP, &mymtop);
	if (ret != 0) {
		fprintf(stderr, "Tape operation OFFL failed.\n");
		exit(1);
	}

	mymtop.mt_count = 1;
	mymtop.mt_op = MTLOAD;
	ret = ioctl(fd, MTIOCTOP, &mymtop);
	if (ret != 0) {
		fprintf(stderr, "Tape operation LOAD failed.\n");
		exit(1);
	} else
		fprintf(stderr, "done\n");

	mymtop.mt_count = 1;
	mymtop.mt_op = MTREW;
	ret = ioctl(fd, MTIOCTOP, &mymtop);
	if (ret != 0) {
		fprintf(stderr, "Tape operation REW failed.\n");
		exit(1);
	}
}

/* parse the commandline options */
void parse_opts(int argc, char *argv[])
{
	int opt, index;
	static struct option long_options[] = {
		{"info",    no_argument, 0, 'i'},
		{"help",    no_argument, 0, 'h'},
		{"version", no_argument, 0, 'v'},
		{"all",     no_argument, 0, 'a'},
		{"device",  no_argument, 0, 'd'},
		{0,         0,           0, 0  }
	};
	static const char option_string[] = "iavhd";

	while ((opt = getopt_long(argc, argv, option_string, long_options,
			       &index)) != -1) {
		switch (opt) {
		case 'd':
			option_d_set = 1;
			break;
		case 'a':
			option_a_set = 1;
			break;
		case 'i':
			option_i_set = 1;
			break;
		case 'h':
			printf(help_text);
			exit(0);
		case 'v':
			printf("%s\n", version_text);
			printf("%s\n", copyright_notice);
			exit(0);
		default:
			fprintf(stderr, "Try 'zgetdump --help' for more"
					" information.\n");
			exit(1);
		}
	}

	/* check if -a and -i options are used correctly and check */
	/* if devicename has been specified                        */

	if ((option_a_set && !option_i_set) || (optind != argc-1)
	    || (option_d_set && option_i_set)) {
		printf(help_text);
		exit(1);


	}
	strcpy(dump_device, argv[optind]);
}

/* Loop along all involved volumes (dump partitions) and either check (for
 * option --info) or pick up dump data                                     */
int mvdump_check_or_copy(int vol_count, struct disk_info vol[])
{
	int i, fd, rc = 1;
	uint64_t data_size, total_size = 0;
	char* temp_devnode;

	for (i = 0; i < vol_count; i++) {
		if (vol[i].status != ONLINE) {
			fprintf(stderr, "============================="
				"=======================\n");
			fprintf(stderr, "ERROR: Dump device %s is not "
				"available.\n", vol[i].bus_id);
			fprintf(stderr, "============================="
				"=======================\n");
			return 1;
		}
		if (vol[i].signature != ACTIVE) {
			fprintf(stderr, "============================="
				"=======================\n");
			fprintf(stderr, "ERROR: Invalid dump data on "
				"%s.\n", vol[i].bus_id);
			fprintf(stderr, "============================="
				"=======================\n");
			return 1;
		}
		if (make_temp_devnode(vol[i].device, &temp_devnode))
			return 1;
		fd = open_block_device(temp_devnode);
		if (fd == -1) {
			free_temp_devnode(temp_devnode);
			return 1;
		}
		if (lseek(fd, vol[i].start_offset, SEEK_SET) !=
		    vol[i].start_offset) {
			perror("Cannot seek on device");
			goto out;
		}
		get_header(fd);
		print_s390_header(IS_MULT_DASD);
		fprintf(stderr, "\nMulti-volume dump: Disk %i (of %i)\n",
				i + 1, vol_count);
		if (option_i_set) {
			data_size = ((vol[i].part_size >> 12) << 12) -
				HEADER_SIZE;
			if (total_size + data_size > header.dh_memory_size) {
				if (lseek(fd, header.dh_memory_size -
					  total_size, SEEK_CUR) == -1) {
					perror("Cannot seek on device");
					goto out;
				}
				fprintf(stderr, "Checking dump contents on "
					"%s\n", vol[i].bus_id);
				if (dump_end_times(fd) == 0) {
					fprintf(stderr, "Dump End Marker "
						"found: "
						"this dump is valid.\n\n");
					rc = 0;
					goto out;
				} else {
					fprintf(stderr, "Dump End Marker not "
						"found: "
						"this dump is NOT valid.\n\n");
					goto out;
				}
			} else if (i == vol_count - 1) {
				fprintf(stderr, "Dump End Marker not found: "
					"this dump is NOT valid.\n\n");
				goto out;
			}
			total_size += data_size;
			fprintf(stderr, "Skipping dump contents on %s\n",
				vol[i].bus_id);
		} else {
			if (i == 0)
				write_header();
			fprintf(stderr, "Reading dump contents from %s",
				vol[i].bus_id);
			if (mvdump_copy(fd, vol[i].part_size, &total_size))
				goto out;
			if ((i == vol_count - 1) ||
			    (total_size == header.dh_memory_size)) {
				rc = check_and_write_end_marker(fd);
				goto out;
			}
		}
		free_temp_devnode(temp_devnode);
		close(fd);
	}
	return 0;
out:
	free_temp_devnode(temp_devnode);
	close(fd);
	return rc;
}

int main(int argc, char *argv[])
{
	uint64_t cur_time, size_limit;
	int vol_count, fd = -1;
	int version, dumper_arch, dasd_mv_flag = 0, block_size, rc;
	int force_specified = 0;
	enum dump_type d_type;
	enum devnode_type type;
	struct disk_info vol[MAX_DUMP_VOLUMES];
	uint32_t cur_volnr;

	rc = 0;
	parse_opts(argc, argv);

	if (option_d_set) {
		fd = open_block_device(dump_device);
		if (fd == -1) {
			rc = 1;
			goto out;
		}
		rc = check_dump_tool_fba(fd, &version, &dumper_arch,
					  &size_limit);
		if (rc)
			goto out;
		if (version >= 0)
			goto is_dump_device;
		else
			rc = check_dump_tool_eckd(fd, &version, &dumper_arch,
						   &dasd_mv_flag, &block_size,
						   &force_specified,
						   &size_limit);
		if (rc)
			goto out;
		if (version >= 0)
			goto is_dump_device;
		fprintf(stderr, "'%s' is no dump device.\n", dump_device);
		rc = 1;
		goto out;

is_dump_device:
		if (dasd_mv_flag) {
			rc = get_mvdump_info(fd, block_size, &vol_count, vol);
			if (rc)
				goto out;
			print_mvdump_info(version, vol_count, vol, size_limit,
					  force_specified);
		} else
			rc = print_dump_info(version, dumper_arch,
					     size_limit);
		goto out; /* do not consider any other options */
	}

	fd = open_dump(dump_device);
	get_header(fd);
	d_type = dev_type(fd);
	if ((d_type == IS_DASD) &&
	    ((header.dh_magic_number == DUMP_MAGIC_LKCD)
	     || (header.dh_magic_number == DUMP_MAGIC_LIVE))) {
		print_lkcd_header(fd);
		exit(0);
	}
	if (d_type != IS_TAPE) {
		type = check_device(dump_device, 0);
		if (type == IS_DEVICE) {
			/* This is a valid block device node, no partition */
			rc = check_dump_tool_eckd(fd, &version, &dumper_arch,
						   &dasd_mv_flag, &block_size,
						   &force_specified,
						   &size_limit);
			if (rc)
				goto out;
			if (!dasd_mv_flag) {
				fprintf(stderr, "Device '%s' specified where"
				" partition is required.\n", dump_device);
				rc = 1;
				goto out;
			} else
				d_type = IS_MULT_DASD;
		} else if ((type == IS_PARTITION) &&
			   (header.dh_mvdump_signature == DUMP_MAGIC_S390)) {
			fprintf(stderr, "'%s' is a multi-volume dump "
				"partition.\nSpecify the corresponding device "
				"node instead.\n", dump_device);
			rc = 1;
			goto out;
		}
	}

	if (dasd_mv_flag) {
		rc = get_mvdump_info(fd, block_size, &vol_count, vol);
		if (rc)
			goto out;
		rc = mvdump_check_or_copy(vol_count, vol);
		goto out;
	}

	if (!option_i_set) {		/* copy the dump to stdout */
		print_s390_header(d_type);
		write_header();
		fprintf(stderr, "Reading dump content ");

		/*	now get_dump returns 1 for all	*/
		/*	except the last tape of a multi-volume dump */

		while (get_dump(fd, d_type) == 1) {
			fprintf(stderr, "\nWaiting for next volume to be "
				"loaded... ");
			load_next(fd);
			get_header(fd);
			print_s390_header(d_type);
		}

		/*	if dev is DASD and dump is copied	*/
		/*	check if the dump is valid		*/

		if (d_type == IS_DASD)
			lseek(fd, header.dh_header_size + header.dh_memory_size,
			      SEEK_SET);

		if (!check_and_write_end_marker(fd))
			goto out;
	} else if (!option_a_set) {		/* "-i" option */
		fprintf(stderr, "\n> \"zgetdump -i\" checks if a dump on "
			"either\n");
		fprintf(stderr, "> a dasd volume or single tape is valid.\n");
		fprintf(stderr, "> If the tape is part of a multi-volume tape "
			"dump,\n");
		fprintf(stderr, "> it checks if it is a valid portion of "
			"the dump.\n");
		print_s390_header(d_type);
		if (d_type == IS_DASD)
			lseek(fd,
			      header.dh_header_size + header.dh_memory_size,
			      SEEK_SET);
		else {
			fprintf(stderr, "Checking if the dump is valid - "
				"this might take a while...\n");
			tape_forwards(fd);
		}
		if (dump_end_times(fd) == 0) {
			fprintf(stderr, "Dump End Marker found: ");
			if (header.dh_volnr != 0)
				fprintf(stderr, "this is a valid part of "
					"a dump.\n\n");
			else
				fprintf(stderr, "this dump is valid.\n\n");
			goto out;
		} else if (d_type == IS_DASD) {
			fprintf(stderr, "Dump End Marker not found: "
				"this dump is NOT valid.\n\n");
			rc = 1;
			goto out;
		} else
			fprintf(stderr, "Checking for End of Volume...\n");
		if (vol_end() != 0) {
			fprintf(stderr, "End of Volume not found: "
				"this dump is NOT valid.\n\n");
			rc = 1;
			goto out;
		} else {
			fprintf(stderr, "Reached End of Volume %i of a "
				"multi-volume tape dump.\n", header.dh_volnr);
			fprintf(stderr, "This part of the dump is valid.\n\n");
			goto out;
		}
	} else {	/* "-i -a" option */
		fprintf(stderr, "\n> \"zgetdump -i -a\" checks if a "
			"multi-volume tape dump is valid.\n");
		fprintf(stderr, "> Please make sure that all volumes are "
			"loaded in sequence.\n");
		if (d_type == IS_DASD) {
			fprintf(stderr, "\"-i -a\" is used for validation of "
				"multi-volume tape dumps.\n\n");
			rc = 1;
			goto out;
		}
		print_s390_header(d_type);
		cur_volnr = header.dh_volnr;
		cur_time = header.dh_tod;
		fprintf(stderr, "\nChecking if the dump is valid - "
			"this might take a while...\n");
		tape_forwards(fd);
		if (dump_end_times(fd) == 0) {
			fprintf(stderr, "Dump End Marker found: "
				"this dump is valid.\n\n");
			goto out;
		} else if (vol_end() != 0) {
			fprintf(stderr, "End of Volume not found: "
				"this dump is NOT valid.\n\n");
			rc = 1;
			goto out;
		}
		while (vol_end() == 0) {
			cur_volnr += 1;
			fprintf(stderr, "Reached End of Volume %i.\n",
				header.dh_volnr);
			fprintf(stderr, "Waiting for Volume %i to be "
				"loaded... ", cur_volnr);
			load_next(fd);
			get_header(fd);
			print_s390_header(d_type);
			if (header.dh_volnr != cur_volnr) {
				fprintf(stderr, "This is not Volume %i\n",
					cur_volnr);
				rc = 1;
				goto out;
			} else if (header.dh_tod != cur_time) {
				fprintf(stderr, "Time stamp of this volume "
					"does not match the previous one.\n");
				rc = 1;
				goto out;
			}
			tape_forwards(fd);
			if (dump_end_times(fd) == 0) {
				fprintf(stderr, "Dump End found: "
					"this dump is valid.\n\n");
				goto out;
			} else if (vol_end() != 0) {
				fprintf(stderr, "End of Volume not found: "
					"this dump is NOT valid.\n\n");
				rc = 1;
				goto out;
			}
		}
	}
out:
	if (fd != -1)
		close(fd);
	return(rc);
}
