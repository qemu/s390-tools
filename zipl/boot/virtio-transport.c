/*
 * VirtIO transport implementation
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

#include "virtio-transport.h"

/***********************************************
 *             Virtio functions                *
 ***********************************************/

static struct virtio_dev console_dev;
static struct vring console_in, console_out;
static char console_works = 0;

static struct vring block;
static char block_works = 0;

static u32 boot_map;

void virtio_stop(void)
{
    while(1)
        early_puts("stop\n");
}

void virtio_panic(const char *string)
{
    early_puts("panic: ");
    early_puts(string);
    early_puts("\n");

    virtio_stop();
}

void virtio_puts(const char *string)
{
    int string_len = 0;
    const char *ns;

    if (!console_works) {
        early_puts("early: ");
        early_puts(string);
        return;
    }

    for(ns = string; *ns; ns++)
        string_len++;

    vring_send_buf(&console_out, (void*)string, string_len, 0);
    vring_wait_reply(&console_out, 0);
}

static void virtio_enum_console(struct virtio_dev *vdev)
{
    console_dev = *vdev;
    struct virtio_vqconfig *vq_in = &vdev->vqconfig[0];
    struct virtio_vqconfig *vq_out = &vdev->vqconfig[1];

    /* We always take the first console */
    if (console_works)
        return;

    /* safety check */
    if (vdev->header->num_vq < 2)
        return;

    vring_init(&console_in, vq_in->num, (void*)(ulong)vq_in->address,
               KVM_S390_VIRTIO_RING_ALIGN);
    vring_init(&console_out, vq_out->num, (void*)(ulong)vq_out->address,
               KVM_S390_VIRTIO_RING_ALIGN);

    vdev->header->status = VIRTIO_CONFIG_S_DRIVER_OK;
    virtio_set_status((unsigned long)vdev->header);

    console_works = 1;
}

static void virtio_enum_block(struct virtio_dev *vdev)
{
    console_dev = *vdev;
    struct virtio_vqconfig *vq = &vdev->vqconfig[0];

    /* We always only use the first block device */
    if (block_works)
        return;

    /* safety check */
    if (vdev->header->num_vq != 1)
        return;

    vring_init(&block, vq->num, (void*)(ulong)vq->address,
               KVM_S390_VIRTIO_RING_ALIGN);

    vdev->header->status = VIRTIO_CONFIG_S_DRIVER_OK;
    virtio_set_status((unsigned long)vdev->header);

    block_works = 1;
}

/*
 * Checks if we can make use of the device.
 *
 * Returns the next device pointer or NULL if done
 */
static void *virtio_enum_dev(void *devptr)
{
    struct virtio_dev_header *dev = devptr;
    struct virtio_dev vdev;

    if (!dev->type)
        return NULL;

    debug_print_addr("enum ptr", devptr);
    debug_print_int("type", dev->type);
    debug_print_int("num_vq", dev->num_vq);
    debug_print_int("feature_len", dev->feature_len);
    debug_print_int("config_len", dev->config_len);

    vdev.header = dev;
    vdev.vqconfig = (struct virtio_vqconfig*)dev->vqconfig;
    vdev.host_features = (char*)&vdev.vqconfig[dev->num_vq];
    vdev.guest_features = &vdev.host_features[dev->feature_len];
    vdev.config = &vdev.guest_features[dev->feature_len];

    switch (dev->type) {
        case VIRTIO_ID_CONSOLE:
            //virtio_reset(dev);
            virtio_enum_console(&vdev);
            break;
        case VIRTIO_ID_BLOCK:
            //virtio_reset(dev);
            virtio_enum_block(&vdev);
            break;
    }

    return &vdev.config[dev->config_len];
}

int virtio_read(ulong sector, void *load_addr)
{
    struct virtio_blk_outhdr out_hdr;
    u8 status;

    /* Tell the host we want to read */
    out_hdr.type = VIRTIO_BLK_T_IN;
    out_hdr.ioprio = 99;
    out_hdr.sector = sector;

    vring_send_buf(&block, &out_hdr, sizeof(out_hdr), VRING_DESC_F_NEXT);

    /* This is where we want to receive data */
    vring_send_buf(&block, load_addr, SECTOR_SIZE, VRING_DESC_F_WRITE |
                   VRING_HIDDEN_IS_CHAIN | VRING_DESC_F_NEXT);

    /* status field */
    vring_send_buf(&block, &status, sizeof(u8), VRING_DESC_F_WRITE |
                   VRING_HIDDEN_IS_CHAIN);

    /* Now we can tell the host to read */
    vring_wait_reply(&block, 0);

    return status;
}

static void virtio_init(void)
{
    long ram_size = sclp_get_ram_size();
    void *virtio_start = (void*)ram_size;
    u64 *stage2_desc = (void*)0x218;
    u64 *tmp_space = (void*)0x8000;

    debug_print_int("ram size", ram_size);

    block_works = 0;
    console_works = 0;

    /* Initialize all subsystems */

    do {
        virtio_start = virtio_enum_dev(virtio_start);
    } while(virtio_start);

    /* Tell common.S about the boot map */
    /* Get MBR */
    virtio_read(0, tmp_space);
    /* Copy boot map identifier to stage2_desc */
    boot_map = tmp_space[5];
    stage2_desc[0] = tmp_space[2];
    stage2_desc[1] = tmp_space[3];

    return;
}

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
    early_puts("_extract_length\n");

    debug_print_int("rec_list1", rec_list1);
    debug_print_int("rec_list2", rec_list2);

    retval = (((rec_list2 >> 32) + 1) & 0xffff) * (rec_list2 >> 48);

    debug_print_int("length", retval);
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
    early_puts("_is_zero_block\n");

    debug_print_int("rec_list1", rec_list1);
    debug_print_int("rec_list2", rec_list2);

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
    int i;

    early_puts("_load_direct\n");
    debug_print_int("rec_list1", rec_list1);
    debug_print_int("rec_list2", rec_list2);
    debug_print_int("subchan_id", subchan_id);
    debug_print_addr("load_addr", load_addr);

    if (sec_len != SECTOR_SIZE) {
        debug_print_int("sector size invalid", sec_len);
        return -1;
    }

    for (i = 0; i < sec_num; i++) {
        if ((i % 100) == 0) {
            virtio_puts(".");
        }
        status = virtio_read(sec, (void*)addr);

        if (status) {
            debug_print_int("load_direct status", status);
            virtio_panic("I/O Error");
        }
        sec++;
        addr += SECTOR_SIZE;
    }

    return addr;
}

/***********************************************
 *                 SCLP fake                   *
 ***********************************************/


/*
 * Subroutine to set up the SCLP interface.
 *
 * Parameters:
 *   R2  = 0 to activate, non-zero to deactivate
 *
 * Returns:
 *   R2  = 0 on success, non-zero on failure
 */
unsigned long _sclp_setup(unsigned long deactivate)
{
    if (deactivate)
        return 0;

    early_puts("_sclp_setup\n");
    virtio_init();
    return 0;
}

/*
 * Subroutine which prints a given text to the SCLP console.
 *
 * Parameters:
 *   R2  = address of nil-terminated EBCDIC text
 *
 * Returns:
 *   R2  = 0 on success, 1 on failure
 */
unsigned long _sclp_print(unsigned char *ebcdic)
{
    unsigned char *ns;
    unsigned char out[] = " ";

    /* Invalid pointer */
    if ((unsigned long)ebcdic & 0xffff0000)
        return 1;

    /* Control character */
    if (e2a[*ebcdic] == 0x07)
        return 1;

    /* Print it out */
    for (ns = ebcdic; *ns; ns++) {
        out[0] = e2a[*ns];
        virtio_puts((char*)out);
    }

    virtio_puts("\r\n");

    return 0;
}

/*
 * Subroutine which reads text from the SCLP console.
 *
 * Parameters:
 *   R2  = 0 for no timeout, non-zero for timeout in (approximated) seconds
 *   R3  = destination address for input
 *
 * Returns:
 *   R2  = 0 on input, 1 on failure, 2 on timeout
 *   R3  = length of input data
 */
unsigned long virtio_sclp_read(unsigned long timeout, char *p)
{
    char t[] = " \n";
    unsigned int i;
    int r;

    debug_print_int("read timeout", timeout);
    virtio_puts("-> ");
    vring_send_buf(&console_in, t, 1, VRING_DESC_F_WRITE);
    r = vring_wait_reply(&console_in, timeout);

    /* timeout while reading */
    if (r) {
        return 2;
    }

    virtio_puts(t);

    /* The caller expects EBCDIC, not ASCII */
    for (i = 0; i < sizeof(e2a); i++) {
        if (e2a[i] == t[0]) {
            p[0] = i;

            debug_print_int("ebcdic input", i);
            return 0;
        }
    }

    virtio_puts("E: e2a\n");

    /* Character not found in the translation table */
    return 1;
}

/*
 * Subroutine to read load parameter.
 *
 * Parameters:
 *   R2  = destination address for load parameter (8 bytes)
 *
 * Returns:
 *   R2  = 0 on success, 1 on failure
 */
unsigned long _sclp_param(u64 *dst)
{
    early_puts("_sclp_param\n");
    *dst = 1;
    return 0;
}
