#
#  Ipl stage 1a loader classic version 
#    Copyright IBM Corp. 2001, 2006.
#    Author(s): Antoinette Kaschner (anto@de.ibm.com),
#
#  startup for ipl at address 0
#  second stage boot loader is loaded to 0x2000 and starts at 0x2008

# The first 24 bytes are loaded by ipl to addresses 0-23 (a PSW and two CCWs).
# Byte 234 to 730 of IPL2 contains 
# seek/search/tic/read CCWs , starting with reading record 4.	 
	
	
	.globl _start
_start:
	.long  0x00000000,0x00000000            # bootlist data 
	.long  0x00000000,0x00000000
	.long  0x07000000+.Lseek1,0x60000006    # seek
.Lsearch1:		
	.long  0x31000000+.Lseek1+2,0x60000005  # search
	.long  0x08000000+.Lsearch1,0x00000000  # tic back to search until found
	.long  0x06000000,0x40000000            # read
	.long  0x07000000+.Lseek2,0x60000006    # seek
.Lsearch2:		
	.long  0x31000000+.Lseek2+2,0x60000005  # search
	.long  0x08000000+.Lsearch2,0x00000000  # tic back
	.long  0x06000000,0x40000000            # read
	.long  0x07000000+.Lseek3,0x60000006    # seek
.Lsearch3:
	.long  0x31000000+.Lseek3+2,0x60000005  # search
	.long  0x08000000+.Lsearch3,0x00000000  # tic back
	.long  0x06000000,0x40000000            # read
	.long  0x07000000+.Lseek4,0x60000006    # seek
.Lsearch4:
	.long  0x31000000+.Lseek4+2,0x60000005  # search
	.long  0x08000000+.Lsearch4,0x00000000  # tic back
	.long  0x06000000,0x40000000            # read
	.long  0x07000000+.Lseek5,0x60000006    # seek
.Lsearch5:	                                                 
	.long  0x31000000+.Lseek5+2,0x60000005  # search
	.long  0x08000000+.Lsearch5,0x00000000  # tic back 
	.long  0x06000000,0x40000000            # read     
	.long  0x07000000+.Lseek6,0x60000006    # seek
.Lsearch6:	                                                 
	.long  0x31000000+.Lseek6+2,0x60000005  # search
        .long  0x08000000+.Lsearch6,0x00000000  # tic back 
        .long  0x06000000,0x40000000            # read     
	.long  0x07000000+.Lseek7,0x60000006    # seek
.Lsearch7:	                                                 
	.long  0x31000000+.Lseek7+2,0x60000005  # search
        .long  0x08000000+.Lsearch7,0x00000000  # tic back 
        .long  0x06000000,0x40000000            # read     
	.long  0x07000000+.Lseek8,0x60000006    # seek
.Lsearch8:	                                                 
	.long  0x31000000+.Lseek8+2,0x60000005  # search
        .long  0x08000000+.Lsearch8,0x00000000  # tic back 
        .long  0x06000000,0x40000000            # read     				
.Lseek1:
	.long  0x00000000,0x00000400           # seekargument for cyl 0 hd 0 rec 4   
.Lseek2:
	.long  0x00000000,0x00000000
.Lseek3:	
	.long  0x00000000,0x00000000
.Lseek4:	
	.long  0,0
.Lseek5:	
	.long  0,0
.Lseek6:				
	.long  0,0
.Lseek7:
	.long  0,0
.Lseek8:	
	.long  0,0	

.Lend:




