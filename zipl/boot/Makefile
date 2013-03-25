# Common definitions
include ../../common.mak

S390_TOOLS_VERSION_EBCDIC := $(shell echo -n $(S390_TOOLS_RELEASE) | dd conv=ebcdic 2> /dev/null | hexdump -v -e '/1 "0x%02x,"' | sed 's/.$$//')

CFLAGS  += -I../include -D__ASSEMBLY__ -m64 \
		-DS390_TOOLS_VERSION_EBCDIC=$(S390_TOOLS_VERSION_EBCDIC)
LDFLAGS = -Os -m64
FILES	:= fba0.bin fba2.bin eckd0.bin eckd1a.bin eckd1b.bin eckd2.bin \
	   virtio1.bin virtio2.bin virtioccw1.bin \
	   eckd2dump.bin tapedump.bin stage3.bin tape0.bin fba2dump.bin \
	   eckd2mvdump.bin

all: data.o data.h tape0.bin

# Prevent make from using some default rules...
%:	%.S

virtio-transport.o:	virtio-transport.c
	echo "$(CC) $(CFLAGS) -Os -g -c -fno-stack-protector -o $@ $<";   \
	$(CC) $(CFLAGS) -Os -g -c -fno-stack-protector -o $@ $<

virtio-ccw-transport.o:	virtio-ccw-transport.c
	echo "$(CC) $(CFLAGS) -Os -g -c -fno-stack-protector -o $@ $<";   \
	$(CC) $(CFLAGS) -Os -g -c -fno-stack-protector -o $@ $<

virtio-loadstage2.o:	virtio-loadstage2.c
	echo "$(CC) $(CFLAGS) -Os -g -c -fno-stack-protector -o $@ $<";   \
	$(CC) $(CFLAGS) -Os -g -c -fno-stack-protector -o $@ $<

virtio-ccw-loadstage2.o:	virtio-ccw-loadstage2.c
	echo "$(CC) $(CFLAGS) -Os -g -c -fno-stack-protector -o $@ $<";   \
	$(CC) $(CFLAGS) -Os -g -c -fno-stack-protector -o $@ $<

virtio-fallback.o:	virtio-fallback.c
	echo "$(CC) $(CFLAGS) -Os -g -c -fno-stack-protector -o $@ $<";   \
	$(CC) $(CFLAGS) -Os -g -c -fno-stack-protector -o $@ $<

sclp-ascii.o:	sclp-ascii.c
	echo "$(CC) $(CFLAGS) -Os -g -c -fno-stack-protector -o $@ $<";   \
	$(CC) $(CFLAGS) -Os -g -c -fno-stack-protector -o $@ $<

%.o:	%.S
	$(CC) $(CFLAGS) -c -o $@ $<

virtio2.exec:	virtio2.o virtio-transport.o virtio-menuparam.o
	echo "$(CC) $(LDFLAGS) -o $@ $$SFLAGS $<"
	$(CC) $(LDFLAGS) -o virtio2.exec virtio2.o virtio-transport.o \
			 virtio-menuparam.o \
			 -nostdlib -Wl,-Ttext,0x2000,-Tbss,0x4500

virtio1.exec:	virtio1.o virtio-transport.o virtio-loadstage2.o virtio-fallback.o
	echo "$(CC) $(LDFLAGS) -o $@ $$SFLAGS $<"
	$(CC) $(LDFLAGS) -o virtio1.exec virtio1.o virtio-transport.o \
			 virtio-loadstage2.o virtio-fallback.o \
			 -nostdlib -Wl,-Ttext,0x9000,-Tbss,0x4500

virtioccw1.exec:	virtio1.o virtio-ccw-transport.o virtio-ccw-loadstage2.o virtio-fallback.o sclp-ascii.o
	echo "$(CC) $(LDFLAGS) -o $@ $$SFLAGS $<"
	$(CC) $(LDFLAGS) -o virtioccw1.exec virtio1.o virtio-ccw-transport.o \
			 virtio-ccw-loadstage2.o \
			 -nostdlib -Wl,-Ttext,0x9000,-Tbss,0x4500

%.exec:	%.o
	@STAGE=$$(                                  \
		echo $@ | awk '                           \
			match($$0,/[0-9]+/){                    \
				print substr($$0,RSTART,RLENGTH)      \
			}                                       \
			/tapedump/{                             \
				print "2"                             \
			}'                                      \
	);                                          \
	case $$STAGE in                             \
		0) SFLAGS="-nostdlib -Wl,-Ttext,0";;      \
		1) SFLAGS="-nostdlib -Wl,-Ttext,0x218";;  \
		2) SFLAGS="-nostdlib -Wl,-Ttext,0x2000";; \
		3) SFLAGS="-nostdlib -Wl,-Ttext,0xA000";; \
	esac;                                       \
	$(LINK) $(LDFLAGS) -o $@ $$SFLAGS $<

%.bin:	%.exec
	$(OBJCOPY) -O binary --only-section=.text --only-section=.rodata $< $@

data.o: $(FILES)
	$(LD) -r -b binary -o data.o $(FILES)

data.h: data.o
	rm -f data.h
	$(NM) data.o | while read ADDR TYPE SYMBOL ; do \
				echo "extern char $$SYMBOL;" >>data.h; done

clean:
	rm -f *.o *.exec *.bin $(FILES) data.o data.h tape0.bin

.PHONY: all clean


# Additional manual dependencies

eckd2.o: common.S menu.S sclp.S
fba2.o: common.S menu.S sclp.S
virtio2.o: common.S menu.S virtio-transport.o
eckd2dump.o: dumpcommon.S
tapedump.o: dumpcommon.S
eckd2mvdump.o: dumpcommon.S
