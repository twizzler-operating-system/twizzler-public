#include <machine/pc-multiboot.h>
#include <machine/memory.h>
#include <debug.h>
#include <arch/x86_64-msr.h>
void serial_init();

extern void _init();
uint64_t x86_64_top_mem;
extern void idt_init(void);
extern void idt_init_secondary(void);

static void proc_init(void)
{
	uint64_t cr0, cr4;
	asm volatile("finit");
	asm volatile("mov %%cr0, %0" : "=r"(cr0));
	cr0 |= (1 << 2);
	cr0 |= (1 << 5);
	cr0 |= (1 << 1);
	cr0 |= (1 << 16);
	cr0 &= ~(1 << 30); // make sure caching is on
	asm volatile("mov %0, %%cr0" :: "r"(cr0));
	
	
	asm volatile("mov %%cr4, %0" : "=r"(cr4));
	//cr4 |= (1 << 7); //enable page global TODO: get this to work
	cr4 |= (1 << 10); //enable fast fxsave etc, sse
	/* TODO: bit 16 of CR4 enables wrfsbase etc instructions.
	 * But we need to check CPUID before using them. */
	//cr4 |= (1 << 16); //enable wrfsbase etc
	asm volatile("mov %0, %%cr4" :: "r"(cr4));



	x86_64_wrmsr(X86_MSR_GS_BASE, 0x87654321, 0);
	x86_64_wrmsr(X86_MSR_KERNEL_GS_BASE, 0x12345678, 0);
	asm volatile("swapgs");

	uint32_t lo, hi;
	uint32_t klo, khi;
	x86_64_rdmsr(X86_MSR_GS_BASE, &lo, &hi);
	x86_64_rdmsr(X86_MSR_KERNEL_GS_BASE, &klo, &khi);
	printk(":: %x %x\n", lo, hi);
	printk("::k %x %x\n", klo, khi);

	for(;;);
}

void x86_64_init(struct multiboot *mth)
{
	idt_init();
	serial_init();

	if(!(mth->flags & MULTIBOOT_FLAG_MEM))
		panic("don't know how to detect memory!");
	printk("%lx\n", mth->mem_upper * 1024 - KERNEL_LOAD_OFFSET);
	x86_64_top_mem = mth->mem_upper * 1024 - KERNEL_LOAD_OFFSET;


	kernel_early_init();
	_init();
	proc_init();
	kernel_main();
}

void x86_64_cpu_secondary_entry(struct processor *proc)
{
	idt_init_secondary();
	proc_init();
	x86_64_lapic_init_percpu();
	assert(proc != NULL);
	processor_secondary_entry();
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

