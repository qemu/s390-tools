#
#  Ipl stage 0 loader
#    Copyright IBM Corp. 2001, 2006.
#    Author(s): Martin Schwidefsky (schwidefsky@de.ibm.com),
#
#  startup for ipl at address 0
#  second stage boot loader is loaded to 0x1000 and starts at 0x1008

# The first 24 byes are loaded by ipl to addresses 0-23 (a PSW and two CCWs).
# The CCWs on 8-23 are used as a continuation of the implicit ipl channel
# program. The fba ipl loader only uses the CCW on 8-15 to load parts of the
# the first block again starting from 0 (the reading starts again at block 0,
# byte 0). 

	.org   0x0
	.globl _start
_start:
	.long  0x00080000,0x80002008           # The first 24 byte are loaded
        .long  0x02000200,0x60000000+.Lend     # by ipl to addresses 0-23.
        .long  0x08000000+.Ltic+512,0x40000000 # Tic to continuation
	.long  0x00000000,0x02000000           # room for load descriptor
        .long  0x00000000,0x00000000
.Ltic:	.long  0x43000000+.Llo0+512,0x40000008 # locate record 0
        .long  0x42002000,0x60000200           # bytes 0-512 of second stage
	.long  0x43000000+.Llo1+512,0x40000008 # locate record 1
	.long  0x42002200,0x60000200           # bytes 512-1024 of 2nd stage
	.long  0x43000000+.Llo2+512,0x40000008 # locate record 2
	.long  0x42002400,0x60000200           # bytes 1024-1536 of 2nd stage
	.long  0x43000000+.Llo3+512,0x40000008 # locate record 3
	.long  0x42002600,0x60000200           # bytes 1536-2048 of 2nd stage
	.long  0x43000000+.Llo4+512,0x40000008 # locate record 4
	.long  0x42002800,0x60000200           # bytes 2048-2560 of 2nd stage
	.long  0x43000000+.Llo5+512,0x40000008 # locate record 5
	.long  0x42002A00,0x60000200           # bytes 2560-3072 of 2nd stage
	.long  0x43000000+.Llo6+512,0x40000008 # locate record 6
	.long  0x42002C00,0x60000200           # bytes 3072-3584 of 2nd stage
	.long  0x43000000+.Llo7+512,0x40000008 # locate record 7
	.long  0x42002E00,0x20000200           # bytes 3584-4096 of 2nd stage
# offset 2 in .Llo[0-7]: block count (unsigned short) = 1
# offset 4 in .Llo[0-7]: block number (unsigned long)
.Llo0:	.long  0x06000001,0x00000000
.Llo1:	.long  0x06000001,0x00000000
.Llo2:	.long  0x06000001,0x00000000
.Llo3:	.long  0x06000001,0x00000000
.Llo4:	.long  0x06000001,0x00000000
.Llo5:	.long  0x06000001,0x00000000
.Llo6:	.long  0x06000001,0x00000000
.Llo7:	.long  0x06000001,0x00000000
.Lend:
