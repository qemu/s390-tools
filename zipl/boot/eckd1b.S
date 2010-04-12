#
#  Ipl stage 1b loader new disk layout
#    Copyright IBM Corp. 2001, 2006.
#    Author(s): Antoinette Kaschner (anto@de.ibm.com),
#
#  startup for ipl at address 0
#  second stage boot loader is loaded to 0x2000 and starts at 0x2008

# The first 24 bytes are loaded by ipl to addresses 0-23 (a PSW and two CCWs).
# IPL1 record TICs to IPL2 record at offset X'218' .Byte 218 to 362 of IPL2 contains 
# seek/search/tic/read CCWs .	 
	
	
	.globl _start
_start:
	.long  0,0                             # bootlist data
	.long  0,0
	.long  0x07000000+.Lseek,0x60000006    # seek  
.Lsearch:	
	.long  0x31000000+.Lseek+2,0x60000005  # search
	.long  0x08000000+.Lsearch,0x00000000  # tic back to search until found
	.long  0x06002000,0x60000000           # read record 4 into storage
	.long  0x06000000,0x60000000
	.long  0x06000000,0x60000000
	.long  0x06000000,0x60000000
	.long  0x06000000,0x60000000
	.long  0x06000000,0x60000000
	.long  0x06000000,0x60000000
	.long  0x06000000,0x60000000
	.long  0x06000000,0x60000000
	.long  0x06000000,0x60000000
	.long  0x06000000,0x60000000
	.long  0x06000000,0x20000000 
	  
		
 .Lseek:
	.long  0x00000000,0x00000400           # seekargument for cyl 0 hd 0 rec 4   
