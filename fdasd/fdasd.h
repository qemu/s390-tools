/*
 * File...........: s390-tools/fdasd/fdasd.h
 * Author(s)......: Volker Sameske <sameske@de.ibm.com>
 *                  Horst Hummel   <Horst.Hummel@de.ibm.com>
 * Copyright IBM Corp. 2001,2007
 */

#ifndef FDASD_H
#define FDASD_H

/*****************************************************************************
 * SECTION: Definitions needed for DASD-API (see dasd.h)                     *
 *****************************************************************************/

#define DASD_IOCTL_LETTER 'D'

#define DASD_PARTN_BITS 2

/* 
 * struct dasd_information_t
 * represents any data about the device, which is visible to userspace.
 *  including foramt and featueres.
 */
typedef struct dasd_information_t {
        unsigned int devno;           /* S/390 devno                         */
        unsigned int real_devno;      /* for aliases                         */
        unsigned int schid;           /* S/390 subchannel identifier         */
        unsigned int cu_type  : 16;   /* from SenseID                        */
        unsigned int cu_model :  8;   /* from SenseID                        */
        unsigned int dev_type : 16;   /* from SenseID                        */
        unsigned int dev_model : 8;   /* from SenseID                        */
        unsigned int open_count; 
        unsigned int req_queue_len; 
        unsigned int chanq_len;       /* length of chanq                     */
        char type[4];                 /* from discipline.name, 'none' for    */
	                              /* unknown                             */
        unsigned int status;          /* current device level                */
        unsigned int label_block;     /* where to find the VOLSER            */
        unsigned int FBA_layout;      /* fixed block size (like AIXVOL)      */
        unsigned int characteristics_size;
        unsigned int confdata_size;
        char characteristics[64];     /* from read_device_characteristics    */
        char configuration_data[256]; /* from read_configuration_data        */
} dasd_information_t;

/* Get information on a dasd device (enhanced) */
#define BIODASDINFO   _IOR(DASD_IOCTL_LETTER,1,dasd_information_t)


/*****************************************************************************
 * SECTION: Further IOCTL Definitions  (see fs.h and hdreq.h)                *
 *****************************************************************************/
#define BLKROGET   _IO(0x12,94) /* get read-only status (0 = read_write) */
#define BLKRRPART  _IO(0x12,95) /* re-read partition table */
#define BLKSSZGET  _IO(0x12,104)/* get block device sector size */
#define BLKGETSIZE64 _IOR(0x12,114,size_t) /* device size in bytes (u64 *arg)*/

/* get device geometry */
#define HDIO_GETGEO		0x0301

/*****************************************************************************
 * SECTION: FDASD internal types                                             *
 *****************************************************************************/

#define DEFAULT_FDASD_CONF "/etc/fdasd.conf" /* default config file */

#define PARTN_MASK ((1 << DASD_PARTN_BITS) - 1)
#define USABLE_PARTITIONS ((1 << DASD_PARTN_BITS) - 1)

#define FDASD_ERROR "fdasd error: "
#define DEVICE "device"
#define DISC   "disc"
#define PART   "part"

#define ALTERNATE_CYLINDERS_USED 0x10

static struct option fdasd_long_options[] = {
	{ "version",     no_argument,       NULL, 'v'},
	{ "auto",        no_argument,       NULL, 'a'},
	{ "silent",      no_argument,       NULL, 's'},
	{ "verbose",     no_argument,       NULL, 'r'},
	{ "label",       required_argument, NULL, 'l'},
	{ "config",      required_argument, NULL, 'c'},
	{ "help",        no_argument,       NULL, 'h'},
	{ "table",       no_argument,       NULL, 'p'},
	{ "volser",      no_argument,       NULL, 'i'},
	{ "keep_volser", no_argument,       NULL, 'k'},
	{ 0,             0,                 0,    0  }
};

/* Command line option abbreviations */
static const char option_string[] = "vasrl:c:hpik";

struct fdasd_options {
	char *device;
	char *volser;
	char *conffile;
};

struct fdasd_options options = {
	NULL,		/* device   */
	NULL,		/* volser   */
	NULL,		/* conffile */
};

typedef struct partition_info {
        u_int8_t           used;
        unsigned long      start_trk;
        unsigned long      end_trk;
        unsigned long      len_trk;
        unsigned long      fspace_trk;
        format1_label_t    *f1;
        struct partition_info *next;
        struct partition_info *prev;
} partition_info_t;


typedef struct config_data {
	unsigned long start;
	unsigned long stop;
} config_data_t;


typedef struct fdasd_anchor {
        int vlabel_changed;
        int vtoc_changed;
	int auto_partition;
	int print_table;
	int print_volser;
	int keep_volser;
	int big_disk;
	int silent;
	int verbose;
	int devno;
	int option_reuse;
	int option_recreate;
	int partno[USABLE_PARTITIONS];
	u_int16_t dev_type;
	unsigned int used_partitions;
	unsigned long label_pos;
        unsigned int  blksize;
        unsigned long fspace_trk;
        format4_label_t  *f4;
        format5_label_t  *f5;
	format7_label_t  *f7;
	format9_label_t  *f9; /* template for all f9 labels */
        partition_info_t *first;
        partition_info_t *last;
        volume_label_t   *vlabel;
	config_data_t confdata[USABLE_PARTITIONS];
	u_int32_t hw_cylinders;
	u_int32_t formatted_cylinders;
} fdasd_anchor_t;

enum offset {lower, upper};

enum fdasd_failure {
	parser_failed,
	unable_to_open_disk,
	unable_to_seek_disk,
	unable_to_read_disk,
	read_only_disk,
        unable_to_ioctl,
	wrong_disk_type,
	wrong_disk_format,
	disk_in_use,
	config_syntax_error,
	vlabel_corrupted,
	dsname_corrupted,
	malloc_failed,
	device_verification_failed,
	volser_not_found
};

#define ERROR_STRING_SIZE 1024
#define INPUT_BUF_SIZE 1024

#endif /* FDASD_H */




