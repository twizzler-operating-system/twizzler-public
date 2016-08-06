
ifneq ($(CONFIG_MACHINE),riscv)
$(error "Architecture riscv64 supports machines: riscv")
endif

QEMU=qemu-system-riscv -kernel $(RISCV)/riscv64-unknown-elf/bin/bbl -append $(BUILDDIR)/kernel
TOOLCHAIN_PREFIX=riscv64-unknown-linux-gnu-
C_SOURCES+=$(addprefix arch/$(ARCH)/,init.c interrupt.c thread.c processor.c memory.c)
ASM_SOURCES+=$(addprefix arch/$(ARCH)/,start.S ctx.S)
$(BUILDDIR)/link.ld: arch/$(ARCH)/link.ld.in machine/$(MACHINE)/include/machine/memory.h
	@echo "[GEN] $@"
	@mkdir -p $(BUILDDIR)
	@BASE=$$(grep "KERNEL_VIRTUAL_BASE" machine/$(MACHINE)/include/machine/memory.h | sed -rn 's/#define KERNEL_VIRTUAL_BASE (0x[0-9a-fA-F])/\1/p') && sed "s/%KERNEL_VIRTUAL_BASE%/$$BASE/g" < $< > $@

