
C_SOURCES+=$(addprefix arch/$(ARCH)/,init.c interrupt.c)
ASM_SOURCES+=$(addprefix arch/$(ARCH)/,start.S ctx.S)
$(BUILDDIR)/link.ld: arch/$(ARCH)/link.ld.in machine/$(MACHINE)/include/machine/memory.h
	@echo "[GEN] $@"
	@mkdir -p $(BUILDDIR)
	@BASE=$$(grep "KERNEL_VIRTUAL_BASE" machine/$(MACHINE)/include/machine/memory.h | sed -rn 's/#define KERNEL_VIRTUAL_BASE (0x[0-9a-fA-F])/\1/p') && sed "s/%KERNEL_VIRTUAL_BASE%/$$BASE/g" < $< > $@

