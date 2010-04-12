#
#  Ipl stage 0 loader
#    Copyright IBM Corp. 2001, 2006.
#    Author(s): Antoinette Kaschner (anto@de.ibm.com),
#
#  startup for ipl at address 0
#  second stage boot loader is loaded to 0x2000 and starts at 0x2008

# The first 24 bytes are loaded by ipl to addresses 0-23 (a PSW and two CCWs).
# The 1st CCW is used to read IPL2 record.
# The 2nd CCW is a TIC to the end of the IPL2 record to a read.

	.org   0x0
	.globl _start
_start:
	.long  0x00080000,0x80002008           # PSW :the first 24 byte are loaded by IPL 
        .long  0x06000218,0x60000090           # read IPL record 2  
        .long  0x08000228,0x00000000           # TIC to seek/search/tic/read at end of IPL2   



