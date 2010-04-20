/*
 * S390 stage 2 loading program
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

unsigned long _sclp_setup(unsigned long deactivate);
unsigned long virtio_load_direct(ulong rec_list1, ulong rec_list2,
				 ulong subchan_id, void *load_addr);
unsigned long _sclp_print(char *ebcdic);
void virtio_zipl_fallback(void);
void virtio_puts(const char *string);

void virtio_load_stage2() {
    u64 *sector = (void*)0x8020;
    char *stage2 = (void*)0x2000;
    void (*stage2_func)(void) = (void*)0x2008;
    ulong *bss = (void*)0x4500;

    /* Initialize virtio and fetch MBR to 0x8000 */
    _sclp_setup(0);

    /* Read in stage2 */
    while (*sector) {
        stage2 = (void*)virtio_load_direct(sector[0], sector[1], 0, stage2);
        sector += 2;

        /* Fallback if no stage2 is installed */
        if ((long)stage2 == -1)
            virtio_zipl_fallback();
    }

    /* Newline so stage2 starts off fresh */
    virtio_puts("\n");

    /* Clear BSS */
    while(1) {
        *bss = 0;
        bss++;
        if ((ulong)bss > 0x6000)
            break;
    }

    /* Call stage2! */
    stage2_func();
}
