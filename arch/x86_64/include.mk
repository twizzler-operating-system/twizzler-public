
ifneq ($(CONFIG_MACHINE),pc)
$(error "Architecture x86_64 supports machines: pc")
endif

ARCH_KERNEL_SUPPORT=FEATURE_SUPPORTED_UNWIND

QEMU=qemu-system-x86_64 -kernel $(BUILDDIR)/kernel
TOOLCHAIN_PREFIX=x86_64-pc-elf-
C_SOURCES+=$(addprefix arch/$(ARCH)/,init.c)
ASM_SOURCES+=$(addprefix arch/$(ARCH)/,start.S ctx.S)
$(BUILDDIR)/link.ld: arch/$(ARCH)/link.ld.in machine/$(MACHINE)/include/machine/memory.h
	@echo "[GEN] $@"
	@mkdir -p $(BUILDDIR)
	@BASE=$$(grep "KERNEL_VIRTUAL_BASE" machine/$(MACHINE)/include/machine/memory.h | sed -rn 's/#define KERNEL_VIRTUAL_BASE (0x[0-9a-fA-F])/\1/p') && sed "s/%KERNEL_VIRTUAL_BASE%/$$BASE/g" < $< > $@

