/*
 * S390 virtio-ccw stage 2 loading program
 *
 * Copyright (c) 2013 SUSE LINUX Products GmbH
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

#define VIRTIO_CCW
struct subchannel_id;
struct vring;
static int run_ccw(struct subchannel_id schid, int cmd, void *ptr, int len);
static struct vring block;
static struct subchannel_id blk_schid;

#define EIO	1
#define EBUSY	2
#include "cio.h"
#include "virtio-transport.h"

#define bool int
#define true 1
#define false 0

/* XXX We don't have LTO available, so let's include c files into
       each other. This gives us strong optimization as well */
#define SCLP_ASCII_INCLUDED
#include "sclp-ascii.c"

static void virtio_panic(const char *string);
static int virtio_read(ulong sector, void *load_addr);
static void virtio_puts(const char *string);
unsigned long virtio_load_direct(ulong rec_list1, ulong rec_list2,
				 ulong subchan_id, void *load_addr);

#define VIRTIO_FALLBACK_INCLUDED
#include "virtio-fallback.c"

static void virtio_panic(const char *string)
{
    virtio_puts(string);
    while (1) { }
}

static void virtio_puts(const char *string)
{
    sclp_print(string);
}

static inline void print_int(const char *desc, u64 addr)
{
    unsigned char *addr_c = (unsigned char*)&addr;
    char out[] = ": 0xffffffffffffffff\n";
    unsigned int i;

    for (i = 0; i < sizeof(addr); i++) {
        fill_hex(&out[4 + (i*2)], addr_c[i]);
    }

    virtio_puts(desc);
    virtio_puts(out);
}

static void drain_irqs(struct subchannel_id schid)
{
    struct irb irb = {};
    while (1) {
        if (tsch(schid, &irb)) {
            return;
        }
    }
}

static int run_ccw(struct subchannel_id schid, int cmd, void *ptr, int len)
{
    struct ccw1 ccw = {};
    struct cmd_orb orb = {};
    struct schib schib;
    int r;

    /* start command processing */
    stsch_err(schid, &schib);
    schib.scsw.ctrl = SCSW_FCTL_START_FUNC;
    msch(schid, &schib);

    /* start subchannel command */
    orb.fmt = 1;
    orb.cpa = &ccw;
    orb.lpm = 0x80;

    ccw.cmd_code = cmd;
    ccw.cda = (long)ptr;
    ccw.count = len;

    r = ssch(schid, &orb);
    /*
     * XXX Wait until device is done processing the CCW. For now we can
     *     assume that a simple tsch will have finished the CCW processing,
     *     but the architecture allows for asynchronous operation
     */
    drain_irqs(blk_schid);
    return r;
}

static int virtio_read_many(ulong sector, void *load_addr, int sec_num)
{
    struct virtio_blk_outhdr out_hdr;
    u8 status;

    /* Tell the host we want to read */
    out_hdr.type = VIRTIO_BLK_T_IN;
    out_hdr.ioprio = 99;
    out_hdr.sector = sector;

    vring_send_buf(&block, &out_hdr, sizeof(out_hdr), VRING_DESC_F_NEXT);

    /* This is where we want to receive data */
    vring_send_buf(&block, load_addr, SECTOR_SIZE * sec_num,
                   VRING_DESC_F_WRITE | VRING_HIDDEN_IS_CHAIN |
                   VRING_DESC_F_NEXT);

    /* status field */
    vring_send_buf(&block, &status, sizeof(u8), VRING_DESC_F_WRITE |
                   VRING_HIDDEN_IS_CHAIN);

    /* Now we can tell the host to read */
    vring_wait_reply(&block, 0);

    drain_irqs(blk_schid);

    return status;
}

int virtio_read(ulong sector, void *load_addr)
{
    return virtio_read_many(sector, load_addr, 1);
}

static void virtio_setup_block(struct subchannel_id schid)
{
    struct vq_info_block info;

    /* XXX need to fetch the 128 from host */
    vring_init(&block, 128, (void*)(100 * 1024 * 1024),
               KVM_S390_VIRTIO_RING_ALIGN);

    info.queue = (void*)(100 * 1024 * 1024);
    info.align = KVM_S390_VIRTIO_RING_ALIGN;
    info.index = 0;
    info.num = 128;

    run_ccw(schid, CCW_CMD_SET_VQ, &info, sizeof(info));
    virtio_set_status(VIRTIO_CONFIG_S_DRIVER_OK);
}

static bool virtio_is_blk(struct subchannel_id schid)
{
    int r;
    struct senseid senseid = {};

    /* run sense id command */
    r = run_ccw(schid, CCW_CMD_SENSE_ID, &senseid, sizeof(senseid));
    if (r) {
        return false;
    }
    if ((senseid.cu_type != 0x3832) || (senseid.cu_model != VIRTIO_ID_BLOCK)) {
        return false;
    }

    return true;
}

static void virtio_setup(void)
{
    struct irb irb;
    int i;
    int r;
    bool found = false;

    blk_schid.one = 1;

    for (i = 0; i < 0x10000; i++) {
        blk_schid.sch_no = i;
        r = tsch(blk_schid, &irb);
        if (r != 3) {
            if (virtio_is_blk(blk_schid)) {
                found = true;
                break;
            }
        }
    }

    if (!found) {
        virtio_panic("No virtio-blk device found!\n");
    }

    virtio_setup_block(blk_schid);
}

void virtio_load_stage2()
{
    sclp_setup();
    virtio_setup();
    virtio_zipl_fallback();
    while (1) { }
}


/********************/


/***********************************************
 *            Assembly callbacks               *
 ***********************************************/

/*
 * parameter
 *   %r2+%r3: blocklist descriptor
 * returns
 *   %r2    : number of bytes (blocksize * number of blocks)
 */
unsigned long virtio_extract_length(ulong rec_list1, ulong rec_list2)
{
    unsigned long retval;

    retval = (((rec_list2 >> 32) + 1) & 0xffff) * (rec_list2 >> 48);

    return retval;
}

/*
 * parameter
 *   %r2+%r3: blocklist descriptor
 * returns
 *   %r2    : == 0 for normal block descriptor
 *            != 0 for zero block descriptor
 */
unsigned long virtio_is_zero_block(ulong rec_list1, ulong rec_list2)
{
    return rec_list1;
}

/*
 * parameter
 *   %r2+%r3:	 recordlist descriptor  CCHHRLLn
 *   %r4    :	 device subchannel id
 *   %r5    :	 load address
 */
unsigned long virtio_load_direct(ulong rec_list1, ulong rec_list2,
				 ulong subchan_id, void *load_addr)
{
    u8 status;
    int sec = rec_list1;
    int sec_num = (((rec_list2 >> 32)+ 1) & 0xffff);
    int sec_len = rec_list2 >> 48;
    ulong addr = (ulong)load_addr;

    if (sec_len != SECTOR_SIZE) {
        return -1;
    }

    virtio_puts(".");
    status = virtio_read_many(sec, (void*)addr, sec_num);
    if (status) {
        virtio_panic("I/O Error");
    }
    addr += sec_num * SECTOR_SIZE;

    return addr;
}
