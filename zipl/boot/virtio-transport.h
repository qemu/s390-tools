/*
 * VirtIO transport helpers
 *
 * Copyright (c) 2010 Alexander Graf <agraf@suse.de>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

// #define DEBUG

#define u8	unsigned char
#define u16	unsigned short
#define u32	unsigned int
#define u64	unsigned long long
#define ulong	unsigned long
typedef long size_t;

#define EIO	1
#define EBUSY	2
#define NULL    0

static inline void *memset(void *s, int c, size_t n)
{
    int i;
    unsigned char *p = s;

    for (i = 0; i < n; i++) {
        p[i] = c;
    }

    return s;
}

static const unsigned char e2a[] = {
    0x00, 0x01, 0x02, 0x03, 0x07, 0x09, 0x07, 0x7F,
    0x07, 0x07, 0x07, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
    0x10, 0x11, 0x12, 0x13, 0x07, 0x0A, 0x08, 0x07,
    0x18, 0x19, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x07, 0x07, 0x1C, 0x07, 0x07, 0x0A, 0x17, 0x1B,
    0x07, 0x07, 0x07, 0x07, 0x07, 0x05, 0x06, 0x07,
    0x07, 0x07, 0x16, 0x07, 0x07, 0x07, 0x07, 0x04,
    0x07, 0x07, 0x07, 0x07, 0x14, 0x15, 0x07, 0x1A,
    0x20, 0xFF, 0x83, 0x84, 0x85, 0xA0, 0x07, 0x86,
    0x87, 0xA4, 0x5B, 0x2E, 0x3C, 0x28, 0x2B, 0x21,
    0x26, 0x82, 0x88, 0x89, 0x8A, 0xA1, 0x8C, 0x07,
    0x8D, 0xE1, 0x5D, 0x24, 0x2A, 0x29, 0x3B, 0x5E,
    0x2D, 0x2F, 0x07, 0x8E, 0x07, 0x07, 0x07, 0x8F,
    0x80, 0xA5, 0x07, 0x2C, 0x25, 0x5F, 0x3E, 0x3F,
    0x07, 0x90, 0x07, 0x07, 0x07, 0x07, 0x07, 0x07,
    0x70, 0x60, 0x3A, 0x23, 0x40, 0x27, 0x3D, 0x22,
    0x07, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67,
    0x68, 0x69, 0xAE, 0xAF, 0x07, 0x07, 0x07, 0xF1,
    0xF8, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E, 0x6F, 0x70,
    0x71, 0x72, 0xA6, 0xA7, 0x91, 0x07, 0x92, 0x07,
    0xE6, 0x7E, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78,
    0x79, 0x7A, 0xAD, 0xAB, 0x07, 0x07, 0x07, 0x07,
    0x9B, 0x9C, 0x9D, 0xFA, 0x07, 0x07, 0x07, 0xAC,
    0xAB, 0x07, 0xAA, 0x7C, 0x07, 0x07, 0x07, 0x07,
    0x7B, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47,
    0x48, 0x49, 0x07, 0x93, 0x94, 0x95, 0xA2, 0x07,
    0x7D, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50,
    0x51, 0x52, 0x07, 0x96, 0x81, 0x97, 0xA3, 0x98,
    0x5C, 0xF6, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
    0x59, 0x5A, 0xFD, 0x07, 0x99, 0x07, 0x07, 0x07,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37,
    0x38, 0x39, 0x07, 0x07, 0x9A, 0x07, 0x07, 0x07,
};

static inline void fill_hex(char *out, unsigned char val)
{
    const char hex[] = "0123456789abcdef";

    out[0] = hex[(val >> 4) & 0xf];
    out[1] = hex[val & 0xf];
}

#ifdef DEBUG
static void early_puts(const char *string);
#endif

static inline void debug_print_int(const char *desc, u64 addr)
{
#ifdef DEBUG
    unsigned char *addr_c = (unsigned char*)&addr;
    char out[] = ": 0xffffffffffffffff\n";
    int i;

    for (i = 0; i < sizeof(addr); i++) {
        fill_hex(&out[4 + (i*2)], addr_c[i]);
    }

    early_puts(desc);
    early_puts(out);
#endif
}

static inline void debug_print_addr(const char *desc, void *p)
{
#ifdef DEBUG
    debug_print_int(desc, (unsigned int)(unsigned long)p);
#endif
}

/***********************************************
 *              SCLP functions                 *
 ***********************************************/

struct sccb_header {
    u16     length;
    u8      function_code;
    u8      control_mask[3];
    u16     response_code;
} __attribute__((packed));

struct read_info_sccb {
    struct  sccb_header header;     /* 0-7 */
    u16     rnmax;                  /* 8-9 */
    u8      rnsize;                 /* 10 */
    u8      _reserved0[24 - 11];    /* 11-15 */
    u8      loadparm[8];            /* 24-31 */
    u8      _reserved1[48 - 32];    /* 32-47 */
    u64     facilities;             /* 48-55 */
    u8      _reserved2[84 - 56];    /* 56-83 */
    u8      fac84;                  /* 84 */
    u8      _reserved3[91 - 85];    /* 85-90 */
    u8      flags;                  /* 91 */
    u8      _reserved4[100 - 92];   /* 92-99 */
    u32     rnsize2;                /* 100-103 */
    u64     rnmax2;                 /* 104-111 */
    u8      _reserved5[4096 - 112]; /* 112-4095 */
} __attribute__((packed, aligned(8)));

#define SCLP_CMDW_READ_SCP_INFO         0x00020001
#define INS_SERVC ".insn rre,0xb2200000,"

/* Perform service call. Returns 0 on success. */
static inline int sclp_servc(ulong command, void *sccb)
{
    int cc;

    asm volatile(
        INS_SERVC "%1,%2\n"
        "ipm     %0\n"
        "srl     %0,28"
        : "=&d" (cc)
	: "d" (command), "a" (sccb)
        : "cc", "memory");

    if (cc == 3) return -EIO;
    if (cc == 2) return -EBUSY;

    return 0;
}

static inline long sclp_get_ram_size(void)
{
    struct read_info_sccb sccb;
    int rc;

    memset(&sccb, 0, sizeof(sccb));
    sccb.header.length = sizeof(sccb);
    sccb.header.function_code = 0x80;
    sccb.header.control_mask[2] = 0x80;
    rc = sclp_servc(SCLP_CMDW_READ_SCP_INFO, &sccb);

    if (rc)
        return -1;

    if (sccb.header.response_code != 0x10)
        return -1;

    return sccb.rnmax << 20;
}

/***********************************************
 *           Hypercall functions               *
 ***********************************************/

#define KVM_S390_VIRTIO_NOTIFY		0
#define KVM_S390_VIRTIO_RESET		1
#define KVM_S390_VIRTIO_SET_STATUS	2

static inline long kvm_hypercall(unsigned long nr, unsigned long param)
{
	register ulong r_nr asm("1") = nr;
	register ulong r_param asm("2") = param;
	register long retval asm("2");

	asm volatile ("diag 2,4,0x500"
		      : "=d" (retval)
		      : "d" (r_nr), "0" (r_param)
		      : "memory", "cc");

	return retval;
}

static inline void virtio_set_status(unsigned long dev_addr)
{
    kvm_hypercall(KVM_S390_VIRTIO_SET_STATUS, dev_addr);
}

static inline void virtio_notify(unsigned long vq_addr)
{
    kvm_hypercall(KVM_S390_VIRTIO_NOTIFY, vq_addr);
}

static inline void virtio_reset(void *dev)
{
    kvm_hypercall(KVM_S390_VIRTIO_RESET, (ulong)dev);
}

static void early_puts(const char *string)
{
#ifdef DEBUG
    virtio_notify((unsigned long)string);
#endif
}

/***********************************************
 *             Virtio functions                *
 ***********************************************/

/* Status byte for guest to report progress, and synchronize features. */
/* We have seen device and processed generic fields (VIRTIO_CONFIG_F_VIRTIO) */
#define VIRTIO_CONFIG_S_ACKNOWLEDGE     1
/* We have found a driver for the device. */
#define VIRTIO_CONFIG_S_DRIVER          2
/* Driver has used its parts of the config, and is happy */
#define VIRTIO_CONFIG_S_DRIVER_OK       4
/* We've given up on this device. */
#define VIRTIO_CONFIG_S_FAILED          0x80

/*
 * VirtIO device layout
 *
 * All VirtIO devices are described after the top of RAM.
 * They follow each other, having this following layout:
 *
 * -- first device --
 *
 * struct virtio_dev_header header
 * struct virtio_vqconfig vqconfig[header->num_vq]
 * u8 host_features[header->feature_len]
 * u8 guest_features[header->feature_len]
 * u8 config[header->config_len]
 *
 * -- second device --
 * (see above)
 */

enum virtio_dev_type {
    VIRTIO_ID_NET = 1,
    VIRTIO_ID_BLOCK = 2,
    VIRTIO_ID_CONSOLE = 3,
    VIRTIO_ID_BALLOON = 5,
};

struct virtio_dev_header {
    enum virtio_dev_type type : 8;
    u8  num_vq;
    u8  feature_len;
    u8  config_len;
    u8  status;
    u8  vqconfig[];
} __attribute__((packed));

struct virtio_vqconfig {
    u64 token;
    u64 address;
    u16 num;
    u8  pad[6];
} __attribute__((packed));

struct virtio_dev {
    struct virtio_dev_header *header;
    struct virtio_vqconfig *vqconfig;
    char *host_features;
    char *guest_features;
    char *config;
};

#define KVM_S390_VIRTIO_RING_ALIGN	4096

#define VRING_USED_F_NO_NOTIFY  1

/* This marks a buffer as continuing via the next field. */
#define VRING_DESC_F_NEXT       1
/* This marks a buffer as write-only (otherwise read-only). */
#define VRING_DESC_F_WRITE      2
/* This means the buffer contains a list of buffer descriptors. */
#define VRING_DESC_F_INDIRECT   4

/* Internal flag to mark follow-up segments as such */
#define VRING_HIDDEN_IS_CHAIN   256

/* Virtio ring descriptors: 16 bytes.  These can chain together via "next". */
struct vring_desc {
    /* Address (guest-physical). */
    u64 addr;
    /* Length. */
    u32 len;
    /* The flags as indicated above. */
    u16 flags;
    /* We chain unused descriptors via this, too */
    u16 next;
} __attribute__((packed));

struct vring_avail {
    u16 flags;
    u16 idx;
    u16 ring[];
} __attribute__((packed));

/* u32 is used here for ids for padding reasons. */
struct vring_used_elem {
    /* Index of start of used descriptor chain. */
    u32 id;
    /* Total length of the descriptor chain which was used (written to) */
    u32 len;
} __attribute__((packed));

struct vring_used {
    u16 flags;
    u16 idx;
    struct vring_used_elem ring[];
} __attribute__((packed));

struct vring {
    unsigned int num;
    int next_idx;
    struct vring_desc *desc;
    struct vring_avail *avail;
    struct vring_used *used;
};

static inline void vring_init(struct vring *vr, unsigned int num, void *p,
                              unsigned long align)
{
    debug_print_addr("init p", p);
    vr->num = num;
    vr->desc = p;
    vr->avail = p + num*sizeof(struct vring_desc);
    vr->used = (void *)(((unsigned long)&vr->avail->ring[num] + align-1)
                & ~(align - 1));

    /* We're running with interrupts off anyways, so don't bother */
    vr->used->flags = VRING_USED_F_NO_NOTIFY;

    debug_print_addr("init vr", vr);
}

static inline void vring_notify(struct vring *vr)
{
    virtio_notify((unsigned long)vr->desc);
}

static inline void vring_send_buf(struct vring *vr, void *p, int len,
                                  int flags)
{
    /* For follow-up chains we need to keep the first entry point */
    if (!(flags & VRING_HIDDEN_IS_CHAIN)) {
        vr->avail->ring[vr->avail->idx % vr->num] = vr->next_idx;
    }

    vr->desc[vr->next_idx].addr = (ulong)p;
    vr->desc[vr->next_idx].len = len;
    vr->desc[vr->next_idx].flags = flags & ~VRING_HIDDEN_IS_CHAIN;
    vr->desc[vr->next_idx].next = ++vr->next_idx;

    /* Chains only have a single ID */
    if (!(flags & VRING_DESC_F_NEXT)) {
        vr->avail->idx++;
    }

    vr->used->idx = vr->next_idx;
}

static inline u64 get_clock(void)
{
    u64 r;

    asm volatile("stck %0" : "=Q" (r) : : "cc");
    return r;
}

static inline ulong get_second(void)
{
    return (get_clock() >> 12) / 1000000;
}


/*
 * Wait for the host to reply.
 *
 * timeout is in seconds if > 0.
 *
 * Returns 0 on success, 1 on timeout.
 */
static inline int vring_wait_reply(struct vring *vr, int timeout)
{
    ulong target_second = get_second() + timeout;
    int r = 0;

    while (vr->used->idx == vr->next_idx) {
        vring_notify(vr);
        if (timeout && (get_second() >= target_second)) {
            r = 1;
            break;
        }
    }

    vr->next_idx = 0;
    vr->desc[0].len = 0;
    vr->desc[0].flags = 0;

    return r;
}

/***********************************************
 *               Virtio block                  *
 ***********************************************/


/*
 * Command types
 *
 * Usage is a bit tricky as some bits are used as flags and some are not.
 *
 * Rules:
 *   VIRTIO_BLK_T_OUT may be combined with VIRTIO_BLK_T_SCSI_CMD or
 *   VIRTIO_BLK_T_BARRIER.  VIRTIO_BLK_T_FLUSH is a command of its own
 *   and may not be combined with any of the other flags.
 */

/* These two define direction. */
#define VIRTIO_BLK_T_IN         0
#define VIRTIO_BLK_T_OUT        1

/* This bit says it's a scsi command, not an actual read or write. */
#define VIRTIO_BLK_T_SCSI_CMD   2

/* Cache flush command */
#define VIRTIO_BLK_T_FLUSH      4

/* Barrier before this op. */
#define VIRTIO_BLK_T_BARRIER    0x80000000

/* This is the first element of the read scatter-gather list. */
struct virtio_blk_outhdr {
        /* VIRTIO_BLK_T* */
        u32 type;
        /* io priority. */
        u32 ioprio;
        /* Sector (ie. 512 byte offset) */
        u64 sector;
};

#define SECTOR_SIZE 512

