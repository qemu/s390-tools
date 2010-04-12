# Common definitions
include ../../common.mak

CPPFLAGS += -I../include -I../boot -I../../include \
	    -DZFCPDUMP_DIR=$(ZFCPDUMP_DIR) \
	    -DZFCPDUMP_IMAGE=$(ZFCPDUMP_IMAGE) -DZFCPDUMP_RD=$(ZFCPDUMP_RD) \
	    -D_FILE_OFFSET_BITS=64
objects = misc.o proc.o error.o scan.o job.o boot.o bootmap.o disk.o \
	  install.o zipl.o
includes = $(wildcard ../include/*.h)

all: zipl

zipl: $(objects)
	$(LINK) -Wl,-z,noexecstack $^ ../boot/data.o -o $@

install: all
	$(INSTALL) -d -m 755 $(BINDIR)
	$(INSTALL) -c zipl $(BINDIR)
	$(INSTALL) -m 755 $(wildcard zipl_helper.*) $(TOOLS_LIBDIR)

clean:
	rm -f *.o zipl

.PHONY: all install clean

# Additional manual dependencies

boot.o: ../boot/data.o ../boot/data.h
