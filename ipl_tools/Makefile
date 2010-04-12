include ../common.mak

CPPFLAGS += -I../include

all: chreipl

objects = main.o getopt.o ccw.o fcp.o system.o sa.o ipl.o
$(objects): chreipl.h

chreipl: $(objects)
	$(LINK) $^ -o $@
	ln -sf chreipl lsreipl
	ln -sf chreipl chshut
	ln -sf chreipl lsshut

clean:
	rm -f *.o lsreipl chreipl chshut lsshut

install: all
	$(INSTALL) -g $(GROUP) -o $(OWNER) -m 755 chreipl $(USRSBINDIR)
	ln -f -s chreipl $(USRSBINDIR)/lsreipl
	ln -f -s chreipl $(USRSBINDIR)/chshut
	ln -f -s chreipl $(USRSBINDIR)/lsshut
	$(INSTALL) -g $(GROUP) -o $(OWNER) -m 644 man/chreipl.8 $(MANDIR)/man8
	$(INSTALL) -g $(GROUP) -o $(OWNER) -m 644 man/lsreipl.8 $(MANDIR)/man8
	$(INSTALL) -g $(GROUP) -o $(OWNER) -m 644 man/lsshut.8 $(MANDIR)/man8
	$(INSTALL) -g $(GROUP) -o $(OWNER) -m 644 man/chshut.8 $(MANDIR)/man8

.PHONY: all install clean

