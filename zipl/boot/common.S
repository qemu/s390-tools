#
#  Common code for the different second stage boot loaders
#    Copyright IBM Corp. 2001, 2006.
#    Author(s): Martin Schwidefsky (schwidefsky@de.ibm.com),
#		Heiko Carstens (heiko.carstens@de.ibm.com)
#

# some definitions from kernel land
__LC_IPLDEV  = 0xC6C
__LC_PANIC_MAGIC= 0xE00

# Error codes shown as address in disabled wait psw

EENABLE_DEV  = 0x00000100 # enable device failed
EDISABLE_DEV = 0x00000101 # disable device failed
ESSCH        = 0x00000102 # start subchannel failed
EWRONGTYPE   = 0x00000200 # wrong ipl type supplied
	.macro stage2_start
#
# Function entry point at 0x2000 is called with C linkage
#   %r2-%r3: load descriptor
#   %r3    : device subchannel id
#   %r4    : load address
#
        basr   %r1,0
        b      _load_blocklist-.(%r1)
	.align 8

#
# Program execution of the second stage boot loader starts at 0x2008
#
        .globl _start
_start: stm    %r0,%r15,0x180           # store all registers 
        basr   %r13,0
0:      l      %r15,3f-0b(%r13)         # load end of stack address

	bas    %r14,_menu-0b(%r13)	# show menu
	sll    %r2,3			# get offset from config number
	lr     %r6,%r2

        l      %r11,0xb8                # load ipl device subchannel id
	lr     %r2,%r11
	bas    %r14,_enable_device-0b(%r13)
	ltr     %r2,%r2			# lpum valid?
	bz     6f-0b(%r13)
	stc    %r2,.Llpm-0b(%r13)	# save lpum
6:
        la     %r10,0
        st     %r10,__LC_PANIC_MAGIC    # clear panic flag
	lm     %r2,%r3,STAGE2_DESC      # get block with configurations
	lr     %r4,%r11
	l      %r5,4f-0b(%r13)
	la     %r12,0(%r5)              # FIXME: provide the correct offset
        bas    %r14,_load_direct-0b(%r13)
	ar     %r6,%r12
        lm     %r2,%r3,8(%r6)           # get block with load descriptors
        lr     %r4,%r11
        lr     %r5,%r12                 # set load address
        bas    %r14,_load_direct-0b(%r13)
	la     %r12,32(%r12)		# skip header entry
1:      lm     %r2,%r3,0(%r12)		# load blocklist container of descriptor
	l      %r4,20(%r12)		# load type
	lhi    %r5,0xf
	nr     %r4,%r5			# only the last four bits are the type
	l      %r5,28(%r12)		# load address
	l      %r6,24(%r12)		# load psw mask
	sll    %r4,2
	l      %r1,5f-0b(%r4,%r13)	# function pointer according to type
	lr     %r4,%r11			# pass subchannel id
        bas    %r14,0(%r1)
2:	la     %r12,32(%r12)		# next load descriptor
        b      1b-0b(%r13)
3:      .long  0x10000-96
4:	.long  0x8000
5:	.long  _bug			# type 0: not allowed
	.long  _execute			# type 1: jump to target
	.long  _load_binary		# type 2: binary file

_bug:
	basr   %r1,0
0:	l      %r2,1f-0b(%r1)
	bas    %r4,_panik-0b(%r1)	# won't return
        br     %r14
1:	.long  EWRONGTYPE


_load_binary:
        stm    %r6,%r15,24(%r15)
        basr   %r13,0                   # base register
0:      s      %r15,1f-0b(%r13)         # create stack frame
        lr     %r12,%r4
	bas    %r14,_load_blocklist-0b(%r13)
        lm     %r6,%r15,120(%r15)
        br     %r14
1:	.long  96
	
_execute:
	basr   %r1,0
0:	st     %r6,0(%r0)		# write supplied psw to address 0
	st     %r5,4(%r0)
	lr     %r2,%r11
	bas    %r14,_disable_device-0b(%r1)
	lpsw   0			# execute what we have loaded

#include "menu.S"

	.endm

#
# The load descriptor is 32 bytes in length and contains 3 entries:
#   offset 0  : a blocklist descriptor (fba/eckd, 64 bit)
#   offset 23 : a type (8 bit)
#   offset 24 : an address (64 bit)
# The meaning of the address and the validity of the blocklst
# depends on the type.
#   type = 1 : load binary, blocklist valid, address = load address
#   type = 2 : load psw, blocklist invalid, address = PSW
#
	.macro blocklist_traverser
# parameter
#   %r2+%r3: blocklist head descriptor
#   %r4    : device subchannel id
#   %r5    : load address
_load_blocklist:
        stm    %r6,%r15,24(%r15)
        basr   %r13,0                   # base register
0:      s      %r15,7f-0b(%r13)         # create stack frame
1:	lr     %r12,%r4                 # save device subchannel id
	lr     %r11,%r5                 # save load address
	lr     %r8,%r2                  # save descriptor
	lr     %r9,%r3
	bas    %r14,_extract_length-0b(%r13)   # get length from descriptor
	lr     %r10,%r2                 # save returned length
	lr     %r2,%r8                  # reload descriptor to %r2/%r3
	lr     %r3,%r9
	lr     %r4,%r12                 # reload device id to %r4
	l      %r5,9f-0b(%r13)          # get memory area for indirect block
        bas    %r14,_load_direct-0b(%r13) # load indirect block
	lr     %r9,%r10                 # (length / 8 - 1) = # direct descr.
	srl    %r9,3
	bctr   %r9,0
	l      %r8,9f-0b(%r13)
2:	clc    0(8,%r8),8f-0b(%r13)     # test block descriptor
	be     6f-0b(%r13)              # descriptor == 0 -> done
	lm     %r2,%r3,0(%r8)           # pass block descriptor
	bas    %r14,_is_zero_block-0b(%r13)    # test for special 0 descriptor
	ltr    %r2,%r2                  # is it a hole to fill with 0?
	bne    4f-0b(%r13)              # no, normal block descriptor
	lm     %r2,%r3,0(%r8)           # pass block descriptor
	bas    %r14,_extract_length-0b(%r13)   # get length from descriptor
	lr     %r3,%r2                  # move length to an odd register
	lr     %r2,%r11                 # move address to an even register
	alr    %r11,%r3                 # add length to load address
	slr    %r5,%r5                  # set length of source to zero
3:	mvcle  %r2,%r4,0                # clear storage
	bo     3b-0b(%r13)	
	b      5f-0b(%r13)
4:	lm     %r2,%r3,0(%r8)           # pass block descriptor
	lr     %r4,%r12                 # pass device subchannel id
        lr     %r5,%r11                 # pass load address
        bas    %r14,_load_direct-0b(%r13) # load indirect block
	lr     %r11,%r2                 # move updated load address
5:	la     %r8,8(%r8)               # next descriptor
	bct    %r9,2b-0b(%r13)
	lm     %r2,%r3,0(%r8)           # load continuation descriptor
	lr     %r4,%r12                 # move device id for next round
	lr     %r5,%r11			# move load address for next round
	clc    0(8,%r8),8f-0b(%r13)     # test continuation descriptor
	bne    1b-0b(%r13)              # != 0 -> load next indirect block
6:      lr     %r2,%r11                 # return updated load address
	lm     %r6,%r15,120(%r15)
        br     %r14
7:      .long  96
8:      .long  0,0
9:      .long  0x8200                   # memory address for indirect blocks

        .endm

	.macro device_fn
#
# Enable I/O on the ipl device. 
#   %r2 : device subchannel id
# Return:
#   %r2 : lpum of subchannel
#
_enable_device:
        stm    %r6,%r15,24(%r15)
        basr   %r13,0                   # base register
0:      s      %r15,1f-0b(%r13)
	lr     %r1,%r2
	l      %r2,4f-0b(%r13)          # set panik code early
        stsch  2f-0b(%r13)
        oi     2f-0b+5(%r13),0x84       # enable ssch and multipath mode
        msch   2f-0b(%r13)
	bnz    _panik-0b(%r13)          # subchannel busy or in error ?
	lctl   %c6,%c6,3f-0b(%r13)      # enable all interrupt classes
	sr     %r2,%r2
	ic     %r2,10+2f-0b(%r13)	# return lpum in r2
        lm     %r6,%r15,120(%r15)
        br     %r14
1:	.long  96
	.align 8
2:	.fill  64,1,0
3:	.long  0xff000000               # CR6 initial value
4:      .long  EENABLE_DEV

#
# Disable I/O on the ipl device.
#   %r2 : device subchannel id
#
_disable_device:
        stm    %r6,%r15,24(%r15)
        basr   %r13,0                   # base register
0:      s      %r15,1f-0b(%r13)
	lr     %r1,%r2
	l      %r2,3f-0b(%r13)          # set panik code early
	lctl   %c6,%c6,2f-0b(%r13)      # disable all interrupt classes
        stsch  2f-0b(%r13)
        ni     2f-0b+5(%r13),0x7B       # disable ssch and multipath mode
        msch   2f-0b(%r13)
	bnz    _panik-0b(%r13)          # subchannel busy or in error ?
        lm     %r6,%r15,120(%r15)
        br     %r14
1:	.long  96
        .align 8
2:	.long  0x00000000               # CR6 (all interrupts classes disabled)
3:      .long  EDISABLE_DEV
	.endm	

	.macro io_subroutines
#
# Start I/O
#   %r2 : device subchannel id
#   %r3 : address of orb
#   %r4 : address of irb
#   %r5 : retry count
#
_ssch:
        stm    %r6,%r15,24(%r15)
        basr   %r13,0                   # base register
0:      s      %r15,6f-0b(%r13)         # create stack frame
	lr     %r12,%r2                 # save subchannel id
	lr     %r11,%r3                 # save orb
	lr     %r10,%r4                 # save irb
	lr     %r9,%r5                  # save retry count
	ic     %r0,.Llpm-0b(%r13)	# copy lpm to orb lpm
	stc    %r0,6(%r3)
1:	lr     %r1,%r12
	ssch   0(%r11)                  # go
        bnz    4f-0b(%r13)              # houston, we have a problem
2:	lr     %r2,%r12                 # call _wait4de with subchannel id
	lr     %r3,%r10                 # and irb address as parameters
	bas    %r14,_wait4de-0b(%r13)   # wait until DE or error
	tm     9(%r10),0xff             # test channel status
	bnz    4f-0b(%r13)
	tm     8(%r10),0xf3             # test device status
	bz     5f-0b(%r13)
	bct    %r9,1b-0b(%r13)          # something went wrong, retry.
4:	l      %r2,7f-0b(%r13)
	bas    %r4,_panik-0b(%r13)      # won't return
5:	lm     %r6,%r15,120(%r15)
        br     %r14
6:	.long  96
7:      .long  ESSCH
.Llpm:
	.byte  0xff
	.align 2

#
# Wait for interrupt subroutine
#   %r2 : device subchannel id
#   %r3 : address of irb
#
_wait4de:
	lr     %r1,%r2
	basr   %r4,0
0:      mvc    0x78(8),5f-0b(%r4)       # set i/o new psw
1:	lpsw   4f-0b(%r4)
2:      c      %r1,0xb8                 # compare subchannel id
        bne    1b-0b(%r4)               # unequal -> continue waiting
	tsch   0(%r3)
	tm     9(%r3),0xff              # test channel status
	bnz    3f-0b(%r4)
	tm     8(%r3),0xf3              # got something unusual ?
	bnz    3f-0b(%r4)
        tm     8(%r3),0x04              # got device end ?
        bz     1b-0b(%r4)               # still busy -> continue waiting
3:      br     %r14
        .align 8
4:	.long  0x020a0000,0x80000000+1b
5:	.long  0x00080000,0x80000000+2b # io new psw

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
        .endm
