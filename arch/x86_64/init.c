#include <machine/pc-multiboot.h>
#include <machine/memory.h>
#include <debug.h>
void serial_init();

extern void _init();
uint64_t x86_64_top_mem;
void x86_64_init(struct multiboot *mth)
{
	serial_init();

	if(!(mth->flags & MULTIBOOT_FLAG_MEM))
		panic("don't know how to detect memory!");
	printk("%lx\n", mth->mem_upper * 1024 - KERNEL_LOAD_OFFSET);
	x86_64_top_mem = mth->mem_upper * 1024 - KERNEL_LOAD_OFFSET;


	kernel_early_init();
	_init();
	kernel_main();
}

#include <processor.h>

void x86_64_write_gdt_entry(struct x86_64_gdt_entry *entry, uint32_t base, uint32_t limit,
		uint8_t access, uint8_t gran)
{
	entry->base_low = base & 0xFFFF;
	entry->base_middle = (base >> 16) & 0xFF;
	entry->base_high = (base >> 24) & 0xFF;
	entry->limit_low = limit & 0xFFFF;
	entry->granularity = ((limit >> 16) & 0x0F) | ((gran & 0x0F) << 4);
	entry->access = access;
}

void x86_64_tss_init(struct processor *proc)
{
	struct x86_64_tss *tss = &proc->arch.tss;
	memset(tss, 0, sizeof(*tss));
	tss->ss0 = 0x10;
	tss->esp0 = 0;
	tss->cs = 0x0b;
	tss->ss = tss->ds = tss->es = 0x13;
	x86_64_write_gdt_entry(&proc->arch.gdt[5], (uint32_t)(uintptr_t)tss, sizeof(*tss), 0xE9, 0);
	x86_64_write_gdt_entry(&proc->arch.gdt[6], ((uintptr_t)tss >> 48) & 0xFFFF, ((uintptr_t)tss >> 32) & 0xFFFF, 0, 0);
	asm volatile("movw $0x2B, %%ax; ltr %%ax" ::: "rax", "memory");
}

void x86_64_gdt_init(struct processor *proc)
{
	memset(&proc->arch.gdt, 0, sizeof(proc->arch.gdt));
	x86_64_write_gdt_entry(&proc->arch.gdt[0], 0, 0, 0, 0);
	x86_64_write_gdt_entry(&proc->arch.gdt[1], 0, 0xFFFFF, 0x9A, 0xA);
	x86_64_write_gdt_entry(&proc->arch.gdt[2], 0, 0xFFFFF, 0x92, 0xA);
	x86_64_write_gdt_entry(&proc->arch.gdt[3], 0, 0xFFFFF, 0xFA, 0xA);
	x86_64_write_gdt_entry(&proc->arch.gdt[4], 0, 0xFFFFF, 0xF2, 0xA);
	proc->arch.gdtptr.limit = sizeof(struct x86_64_gdt_entry) * 8 - 1;
	proc->arch.gdtptr.base = (uintptr_t)&proc->arch.gdt;
	asm volatile("lgdt (%0)" :: "r"(&proc->arch.gdtptr));
}

void arch_processor_init(struct processor *proc)
{
	x86_64_gdt_init(proc);
	x86_64_tss_init(proc);
}

