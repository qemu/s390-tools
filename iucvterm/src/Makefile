#! /usr/bin/make -f

include ../../common.mak

ifndef GETTEXT_TEXTDOMAIN
GETTEXT_TEXTDOMAIN = iucvterm
endif

CPPFLAGS += -I../include -I../../include
CPPFLAGS += -DUSE_NLS -DGETTEXT_TEXTDOMAIN=\"$(GETTEXT_TEXTDOMAIN)\"
#CPPFLAGS += -D__DEBUG__

PROGRAMS = iucvconn iucvtty

all: $(PROGRAMS)
check:
install:
	for prg in $(PROGRAMS); do \
	  $(INSTALL) -g $(GROUP) -o $(OWNER) -m 755 $$prg $(USRBINDIR) ; \
	done

clean:
	-rm -f *.o $(PROGRAMS)

iucvconn: iucvconn.o getopt.o auditlog.o functions.o

iucvtty: LDLIBS = -lutil
iucvtty: iucvtty.o getopt.o auditlog.o functions.o

.PHONY: install clean
