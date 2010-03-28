/*
 * S390 stage 2 boot loader for VirtIO
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

STAGE2_DESC = 0x218
#define VIRTIO
#define SCSI
#define BIT64

#include "common.S"
# expand the common start code from iplcommon.s
        stage2_start

# expand blocklist traversing code from iplcommon.s
        blocklist_traverser

/* Don't provide code for enable/disable device */

_enable_device:
_disable_device:
	lhi	%r2, 1
        br	%r14

/*
 * For some functions we need to ensure they get called correctly. So
 * let's provide a common wrapper so we don't have to change the rest
 * of the code.
 */
#define wrap_func(name)							\
name:;									\
	j	virtio ## name

wrap_func(_load_direct)
wrap_func(_extract_length)
wrap_func(_is_zero_block)
	

_sclp_read:
	stm    %r6,%r15,FRAME_R6(%r15)			/* save registers */
	ahi    %r15,-FRAME_SIZE				/* create stack frame */

	bras   %r14,virtio_sclp_read
	lhi    %r3,1					/* read: always char */

	lm     %r6,%r15,(FRAME_SIZE+FRAME_R6)(%r15)	/* restore registers */
	br     %r14

.Llpm:
	.byte  0x00
	.align 2

#
# Panik routine. Loads a disabled wait psw
#   %r2 : panik code
#
_panik:
	basr   %r1,0
0:	st     %r2,1f-0b+4(%r1)         # store code in address part of psw
	lpsw   1f-0b(%r1)
	.align 8
1:	.long  0x000a0000,0x00000000
