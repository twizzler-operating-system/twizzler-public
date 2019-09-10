PORTS=bash

PORTDIR=$(BUILDDIR)/us/ports
SYSROOT=$(BUILDDIR)/us/sysroot


$(PORTDIR)/%.tar.gz:
	@mkdir -p $(dir $@)
	wget $(URL_$*) -O $@


include $(addprefix us/ports/,$(addsuffix .mk,$(PORTS)))


ports: $(TARGETS)
