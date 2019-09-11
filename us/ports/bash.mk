
URL_bash=https://ftp.gnu.org/gnu/bash/bash-5.0.tar.gz
DIR_bash=bash-5.0


TARGETS+=$(SYSROOT)/usr/bin/bash

$(SYSROOT)/usr/bin/bash: $(PORTDIR)/$(DIR_bash)/bash
	$(MAKE) -C $(PORTDIR)/$(DIR_bash) install DESTDIR=$(shell pwd)/$(SYSROOT)
	@touch $@

SRCS_bash=$(shell find $(PORTDIR)/$(DIR_bash))

$(PORTDIR)/$(DIR_bash)/bash: $(PORTDIR)/$(DIR_bash)/Makefile $(SRCS_bash) $(SYSLIBS)
	$(MAKE) -C $(PORTDIR)/$(DIR_bash)

bash_relink: $(PORTDIR)/$(DIR_bash)/Makefile
	@cd $(dir $<) && touch alias.c
	$(MAKE) -C $(PORTDIR)/$(DIR_bash)

bash_clean:
	$(MAKE) -C $(PORTDIR)/$(DIR_bash) uninstall DESTDIR=$(shell pwd)/$(SYSROOT)
	-rm -rf $(PORTDIR)/$(DIR_bash)

$(PORTDIR)/$(DIR_bash)/Makefile: $(PORTDIR)/bash.tar.gz
	cd $(PORTDIR) && rm -rf $(DIR_bash)
	cd $(PORTDIR) && tar xf $(notdir $<)
	cd $(PORTDIR)/$(DIR_bash) && sed -i 's/| sortix\*/| sortix\* | twizzler\*/g' support/config.sub
	cd $(PORTDIR)/$(DIR_bash) && ./configure --prefix=/usr --without-bash-malloc --host=$(TWIZZLER_TRIPLET) --enable-static-link --with-curses


