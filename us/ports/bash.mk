
URL_bash=https://ftp.gnu.org/gnu/bash/bash-5.0.tar.gz
DIR_bash=bash-5.0


TARGETS+=$(SYSROOT)/usr/bin/bash

$(SYSROOT)/usr/bin/bash: $(PORTDIR)/$(DIR_bash)/bash
	$(MAKE) -C $(PORTDIR)/$(DIR_bash) install DESTDIR=$(shell pwd)/$(SYSROOT)
	@touch $@

$(PORTDIR)/$(DIR_bash)/bash: $(PORTDIR)/$(DIR_bash)/Makefile
	$(MAKE) -C $(PORTDIR)/$(DIR_bash)

$(PORTDIR)/$(DIR_bash)/Makefile: $(PORTDIR)/bash.tar.gz
	cd $(PORTDIR) && rm -rf $(DIR_bash)
	cd $(PORTDIR) && tar xf $(notdir $<)
	cd $(PORTDIR)/$(DIR_bash) && sed -i 's/| sortix\*/| sortix\* | twizzler\*/g' support/config.sub
	cd $(PORTDIR)/$(DIR_bash) && ./configure --prefix=/usr --without-bash-malloc --host=$(TWIZZLER_TRIPLET)


