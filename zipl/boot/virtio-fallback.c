/*
 * QEMU S390 zipl interpreter
 *
 * Copyright (c) 2009 Alexander Graf <agraf@suse.de>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, see <http://www.gnu.org/licenses/>.
 */

#include "virtio-transport.h"

#define uint8_t			unsigned char
#define uint16_t		unsigned short
#define uint32_t		unsigned int
#define uint64_t		unsigned long long

// #define DEBUG_FALLBACK

#ifdef DEBUG_FALLBACK
#define dputs(txt) \
    do { virtio_puts("zipl: " txt); } while (0)
#else
#define dputs(fmt, ...) \
    do { } while (0)
#endif

void virtio_panic(const char *string);
int virtio_read(ulong sector, void *load_addr);
void *virtio_load_direct(ulong rec_list1, ulong rec_list2,
			 ulong subchan_id, void *load_addr);
void virtio_puts(const char *string);

struct scsi_blockptr {
    uint64_t blockno;
    uint16_t size;
    uint16_t blockct;
    uint8_t reserved[4];
} __attribute__ ((packed));

struct component_entry {
    struct scsi_blockptr data;
    uint8_t pad[7];
    uint8_t component_type;
    uint64_t load_address;
} __attribute((packed));

struct component_header {
    uint8_t magic[4];
    uint8_t type;
    uint8_t reserved[27];
} __attribute((packed));

struct mbr {
    uint8_t magic[4];
    uint32_t version_id;
    uint8_t reserved[8];
    struct scsi_blockptr blockptr;
} __attribute__ ((packed));

#define ZIPL_MAGIC			"zIPL"

#define ZIPL_COMP_HEADER_IPL		0x00
#define ZIPL_COMP_HEADER_DUMP		0x01

#define ZIPL_COMP_ENTRY_LOAD		0x02
#define ZIPL_COMP_ENTRY_EXEC		0x01

/* Scratch space */
uint8_t *sec;

/* Check for ZIPL magic. Returns 0 if not matched. */
static int zipl_magic(uint8_t *ptr)
{
    uint32_t *p = (void*)ptr;
    uint32_t *z = (void*)ZIPL_MAGIC;

    if (*p != *z) {
        debug_print_int("invalid magic", *p);
        virtio_panic("invalid magic");
    }

    return 1;
}

static int zipl_load_segment(struct component_entry *entry)
{
    const int max_entries = (SECTOR_SIZE / sizeof(struct scsi_blockptr));
    struct scsi_blockptr *bprs = (void*)sec;
    uint64_t blockno;
    void *address;
    int i;

    blockno = entry->data.blockno;
    address = (void*)entry->load_address;

    debug_print_int("loading segment at block", blockno);
    debug_print_addr("addr", address);

    do {
        if (virtio_read(blockno, (uint8_t *)bprs)) {
            debug_print_int("failed reading bprs at", blockno);
            goto fail;
        }

        for (i = 0;; i++) {
            u64 *cur_desc = (void*)&bprs[i];

            blockno = bprs[i].blockno;
            if (!blockno)
                break;

            /* we need the updated blockno for the next indirect entry in the
               chain, but don't want to advance address */
            if (i == (max_entries - 1))
                break;

            address = virtio_load_direct(cur_desc[0], cur_desc[1], 0, address);
            if ((long)address == -1)
                goto fail;
        }
    } while (blockno);

    return 0;

fail:
    virtio_puts("failed loading segment\n");
    return -1;
}

/* Run a zipl program */
static int zipl_run(struct scsi_blockptr *pte)
{
    struct component_header *header;
    struct component_entry *entry;
    void (*ipl)(void);
    uint8_t tmp_sec[SECTOR_SIZE];

    virtio_read(pte->blockno, tmp_sec);
    header = (struct component_header *)tmp_sec;

    if (!zipl_magic(tmp_sec))
        goto fail;

    if (header->type != ZIPL_COMP_HEADER_IPL)
        goto fail;

    dputs("start loading images\n");

    /* Load image(s) into RAM */
    entry = (struct component_entry *)(&header[1]);
    while (entry->component_type == ZIPL_COMP_ENTRY_LOAD) {
        if (zipl_load_segment(entry) < 0)
            goto fail;

        entry++;

        if ((uint8_t*)(&entry[1]) > (tmp_sec + SECTOR_SIZE))
            goto fail;
    }

    if (entry->component_type != ZIPL_COMP_ENTRY_EXEC)
        goto fail;

    /* Ensure the guest output starts fresh */
    virtio_puts("\n");

    /* And run the OS! */
    ipl = (void*)(entry->load_address & 0x7fffffff);
    debug_print_addr("set IPL addr to", ipl);
    /* should not return */
    ipl();

    return 0;

fail:
    virtio_puts("failed running zipl\n");
    return -1;
}

int zipl_load(void)
{
    struct mbr *mbr = (void*)sec;
    uint8_t *ns, *ns_end;
    int program_table_entries = 0;
    int pte_len = sizeof(struct scsi_blockptr);
    struct scsi_blockptr *prog_table_entry;
    const char *error = "";

    /* Grab the MBR */
    virtio_read(0, (void*)mbr);

    dputs("checking magic\n");

    if (!zipl_magic(mbr->magic)) {
        error = "zipl_magic 1";
        goto fail;
    }

    debug_print_int("program table", mbr->blockptr.blockno);

    /* Parse the program table */
    if (virtio_read(mbr->blockptr.blockno, sec)) {
        error = "virtio_read";
        goto fail;
    }

    if (!zipl_magic(sec)) {
        error = "zipl_magic 2";
        goto fail;
    }

    ns_end = sec + SECTOR_SIZE;
    for (ns = (sec + pte_len); (ns + pte_len) < ns_end; ns++) {
        prog_table_entry = (struct scsi_blockptr *)ns;
        if (!prog_table_entry->blockno)
            break;

        program_table_entries++;
    }

    debug_print_int("program table entries", program_table_entries);

    if (!program_table_entries)
        goto fail;

    /* Run the default entry */

    prog_table_entry = (struct scsi_blockptr *)(sec + pte_len);

    return zipl_run(prog_table_entry);

fail:
    virtio_puts("failed loading zipl: ");
    virtio_puts(error);
    virtio_puts("\n");
    return -1;
}


void virtio_zipl_fallback(void)
{
    sec = (void*)0x8000;

    virtio_puts("No virtio zipl found. Falling back to manual default entry loading\n");
    if (zipl_load() < 0)
        virtio_puts("Failed to load OS from hard disk\n");
}


