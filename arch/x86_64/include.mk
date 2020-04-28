
ifneq ($(CONFIG_MACHINE),pc)
$(error "Architecture x86_64 supports machines: pc")
endif

ARCH_KERNEL_SUPPORT=FEATURE_SUPPORTED_UNWIND

CORE_CFLAGS+=-mno-red-zone -mno-sse -mcmodel=kernel -mno-avx
LDFLAGS+=-mcmodel=kernel -Wl,-z,max-page-size=4096 -Wl,-z,common-page-size=4096

QEMU=qemu-system-x86_64 -cpu host,migratable=false,host-cache-info=true,host-phys-bits -machine q35,kernel-irqchip=split -device intel-iommu,intremap=on  -m 1024 # -kernel #$(BUILDDIR)/kernel # -initrd $(BUILDDIR)/initrd.tar -cpu host

C_SOURCES+=$(addprefix arch/$(ARCH)/,init.c processor.c memory.c debug.c pit.c ioapic.c acpi.c madt.c idt.c entry.c vmx.c virtmem.c hpet.c thread.c rdrand.c object.c intel_iommu.c x2apic.c kconf.c)

ASM_SOURCES+=$(addprefix arch/$(ARCH)/,start.S ctx.S trampoline.S interrupt.S gate.S)

# This file mucks with gs, which is used in stack-smashing detection
CFLAGS_arch_x86_64_init.c=-fno-stack-protector
CFLAGS_arch_x86_64_vmx.c=-fno-stack-protector

$(BUILDDIR)/link.ld: arch/$(ARCH)/link.ld.in machine/$(MACHINE)/include/machine/memory.h
	@echo "[GEN] $@"
	@mkdir -p $(BUILDDIR)
	@BASE=$$(grep "KERNEL_VIRTUAL_BASE" machine/$(MACHINE)/include/machine/memory.h | sed -rn 's/#define KERNEL_VIRTUAL_BASE (0x[0-9a-fA-F])/\1/p') && sed "s/%KERNEL_VIRTUAL_BASE%/$$BASE/g" < $< > $@
	@BASE=$$(grep "KERNEL_PHYSICAL_BASE" machine/$(MACHINE)/include/machine/memory.h | sed -rn 's/#define KERNEL_PHYSICAL_BASE (0x[0-9a-fA-F])/\1/p') && sed -i "s/%KERNEL_PHYSICAL_BASE%/$$BASE/g" $@
	@BASE=$$(grep "KERNEL_LOAD_OFFSET" machine/$(MACHINE)/include/machine/memory.h | sed -rn 's/#define KERNEL_LOAD_OFFSET (0x[0-9a-fA-F])/\1/p') && sed -i "s/%KERNEL_LOAD_OFFSET%/$$BASE/g" $@


$(BUILDDIR)/crti.o : arch/x86_64/crti.S $(CONFIGFILE)
	@echo "[AS]  $@"
	@mkdir -p $(@D)
	@$(TOOLCHAIN_PREFIX)gcc $(ASFLAGS) -c $< -o $@

$(BUILDDIR)/crtn.o : arch/x86_64/crtn.S $(CONFIGFILE)
	@echo "[AS]  $@"
	@mkdir -p $(@D)
	@$(TOOLCHAIN_PREFIX)gcc $(ASFLAGS) -c $< -o $@

CRTI=$(BUILDDIR)/crti.o
CRTN=$(BUILDDIR)/crtn.o

$(BUILDDIR)/kernel.stage1: $(CRTI) $(CRTN)
$(BUILDDIR)/kernel: $(CRTI) $(CRTN)

bootiso: $(BUILDDIR)/kernel userspace
	@mkdir -p $(BUILDDIR)/boot/grub
	@cp machine/pc/grub.cfg $(BUILDDIR)/boot/grub
	@-rm $(BUILDDIR)/boot.iso
	cd $(BUILDDIR); grub-mkrescue -o boot.iso kernel us/root.tar .

test-bochs: bootiso
	bochs -f machine/pc/bochsrc.txt

