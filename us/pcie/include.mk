pcie_srcs=$(addprefix us/pcie/,pcie.c)

pcie_objs=$(addprefix $(BUILDDIR)/,$(pcie_srcs:.c=.o))
pcie_deps=$(addprefix $(BUILDDIR)/,$(pcie_srcs:.c=.d))

pcie_all: $(BUILDDIR)/us/pcie/pcie

$(BUILDDIR)/us/pcie/pcie: $(pcie_objs) $(US_LIBDEPS)
	@echo "[CLD] $@"
	@$(TWZCC) $(US_LDFLAGS) $(US_CFLAGS) -o $@ -nostdlib $(US_PRELINK) $< $(US_POSTLINK) -MD

-include $(pcie_deps)

$(BUILDDIR)/us/data/pcieids.obj: /usr/share/hwdata/pci.ids $(BUILDDIR)/utils/file2obj
	@echo [OBJ] $@
	@$(BUILDDIR)/utils/file2obj -i $< -o $@ -p rh

TWZOBJS+=$(BUILDDIR)/us/data/pcieids.obj

.PHONY: pcie_all
