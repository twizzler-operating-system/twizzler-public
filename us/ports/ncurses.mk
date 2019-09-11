
URL_ncurses=ftp://ftp.gnu.org/gnu/ncurses/ncurses-6.1.tar.gz
DIR_ncurses=ncurses-6.1


TARGETS+=$(SYSROOT)/usr/bin/ncurses

ncurses $(SYSROOT)/usr/lib/libcurses.a: $(PORTDIR)/$(DIR_ncurses)/lib/libcurses.a
	$(MAKE) -C $(PORTDIR)/$(DIR_ncurses) install DESTDIR=$(shell pwd)/$(SYSROOT)
	@touch $@

SRCS_ncurses=$(shell find $(PORTDIR)/$(DIR_ncurses))

$(PORTDIR)/$(DIR_ncurses)/lib/libcurses.a: $(PORTDIR)/$(DIR_ncurses)/Makefile $(SRCS_ncurses) $(SYSLIBS)
	$(MAKE) -C $(PORTDIR)/$(DIR_ncurses)

ncurses_clean:
	$(MAKE) -C $(PORTDIR)/$(DIR_ncurses) uninstall DESTDIR=$(shell pwd)/$(SYSROOT)
	-rm -rf $(PORTDIR)/$(DIR_ncurses)

$(PORTDIR)/$(DIR_ncurses)/Makefile: $(PORTDIR)/ncurses.tar.gz
	cd $(PORTDIR) && rm -rf $(DIR_ncurses)
	cd $(PORTDIR) && tar xf $(notdir $<)
	cp us/ports/config.sub $(PORTDIR)/$(DIR_ncurses)/config.sub
	cd $(PORTDIR)/$(DIR_ncurses) && ./configure --prefix=/usr --datadir=/usr/share --with-default-terminfo-dir=/usr/share/terminfo --host=$(TWIZZLER_TRIPLET)


