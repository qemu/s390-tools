#
#  Ipl stage 1 loader
#    Copyright IBM Corp. 2001, 2006.
#    Author(s): Antoinette Kaschner (anto@de.ibm.com),
#
#  startup for ipl at address 0
#  second stage boot loader is loaded to 0x2000 and starts at 0x2008

# The first 24 bytes are loaded by ipl to addresses 0-23 (a PSW and two CCWs).
# IPL1 record TICs to IPL2 record at offset X'68' .Byte 0 to 68 of IPL2 contains boot load data.
# The seek/search/tic/read CCWs at the end of IPL2 record reads  record 2 on disk into storage at
# location X200.	 
	
	
		.org   0x18
	.long  0x00000000,0x00000000           # contains boot load data 
	.long  0x00000000,0x00000000           # contains boot load data 
	.long  0x00000000,0x00000000           # contains boot load data 
	.long  0x00000000,0x00000000           # contains boot load data 
	.long  0x00000000,0x00000000           # contains boot load data 
	.long  0x00000000,0x00000000           # contains boot load data 
	.long  0x00000000,0x00000000           # contains boot load data 
	.long  0x00000000,0x00000000           # contains boot load data 
	.long  0x00000000,0x00000000           # contains boot load data 
	.long  0x00000000,0x00000000           # contains boot load data 
	.long  0x00000000,0x00000000           # contains boot load data 
	.long  0x00000000,0x00000000           # contains boot load data 
	.long  0x00000000,0x00000000           # contains boot load data 
	.long  0x07600006,0x00000000+.Lseek    # seek  
.Lsearch:	
	.long  0x31600005,0x00000000+.Lseek+2  # search
	.long  0x08000000,0x00000000+.Lsearch  # tic back to search until found
	.long  0x06200090,0x00000200           # read record 2 into storage
 .Lseek:
	.long  0x00000000,0x00000200           # seekargument for cyl 0 hd 0 rec 2   
