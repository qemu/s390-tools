#==============================================================================
# Makefile for zSeries configuration utilities.
#==============================================================================
include ../common.mak

SCRIPTS	= lsdasd lstape lscss chccwdev lsqeth lszfcp lschp chchp lszcrypt \
	  chzcrypt lsluns cio_ignore znetconf
MANPAGES= lsdasd.8 lstape.8 lscss.8 chccwdev.8 lsqeth.8 lszfcp.8 lschp.8 \
	  chchp.8 lszcrypt.8 chzcrypt.8 lsluns.8 cio_ignore.8 znetconf.8

all:

clean:

install:	install-scripts install-manpages
	$(INSTALL) -g $(GROUP) -o $(OWNER) -m 755 lsznet.raw $(TOOLS_LIBDIR)
	$(INSTALL) -g $(GROUP) -o $(OWNER) -m 755 znetcontrolunits \
		$(TOOLS_LIBDIR)

install-scripts:	$(SCRIPTS)
	@for i in $^; do \
		cat $$i | \
		sed -e 's+%S390_TOOLS_VERSION%+$(S390_TOOLS_RELEASE)+' \
		>$(BINDIR)/$$i; \
		chown $(OWNER).$(GROUP) $(BINDIR)/$$i; \
		chmod 755 $(BINDIR)/$$i; \
	done

install-manpages:	$(MANPAGES)
	@if [ ! -d $(MANDIR) ]; then \
		mkdir -p $(MANDIR)/man8; \
		chown $(OWNER).$(GROUP) $(MANDIR); \
		chown $(OWNER).$(GROUP) $(MANDIR)/man8; \
		chmod 755 $(MANDIR); \
		chmod 755 $(MANDIR)/man8; \
	fi; \
	for i in $^; do \
		install -o $(OWNER) -g $(GROUP) -m 644 $$i $(MANDIR)/man8; \
	done

.PHONY: all install clean install-scripts install-manpages
