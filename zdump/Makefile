include ../common.mak

CPPFLAGS += -D_FILE_OFFSET_BITS=64 -I../include

all: zgetdump

zgetdump.o: zgetdump.h
zgetdump: zgetdump.o

install: all
	$(INSTALL) -d -m 755 $(MANDIR)/man8 $(BINDIR)
	$(INSTALL) -m 755 zgetdump $(BINDIR)
	$(INSTALL) -m 644 zgetdump.8 $(MANDIR)/man8 

clean:
	rm -f *.o *~ zgetdump core

.PHONY: all install clean
