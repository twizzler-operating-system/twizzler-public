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
	cr0 &= ~(1 << 29); // make sure caching is on
	asm volatile("mov %0, %%cr0" :: "r"(cr0));
	
	
	asm volatile("mov %%cr4, %0" : "=r"(cr4));
	//cr4 |= (1 << 7); //enable page global TODO: get this to work
	cr4 |= (1 << 10); //enable fast fxsave etc, sse
	/* TODO: bit 16 of CR4 enables wrfsbase etc instructions.
	 * But we need to check CPUID before using them. */
	//cr4 |= (1 << 16); //enable wrfsbase etc
	asm volatile("mov %0, %%cr4" :: "r"(cr4));

	/* enable fast syscall extension */
	uint32_t lo, hi;
	x86_64_rdmsr(X86_MSR_EFER, &lo, &hi);
	lo |= X86_MSR_EFER_SYSCALL;
	x86_64_wrmsr(X86_MSR_EFER, lo, hi);

}

void x86_64_init(struct multiboot *mth)
{
	idt_init();
	serial_init();
	proc_init();

	if(!(mth->flags & MULTIBOOT_FLAG_MEM))
		panic("don't know how to detect memory!");
	printk("%lx\n", mth->mem_upper * 1024 - KERNEL_LOAD_OFFSET);
	x86_64_top_mem = mth->mem_upper * 1024 - KERNEL_LOAD_OFFSET;


	kernel_early_init();
	_init();
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

void user_test()
{
	asm volatile("movq $0x1234, %%rax ; syscall; " ::: "rax");
	for(;;);
}

extern int initial_boot_stack;
extern void x86_64_syscall_entry_from_userspace();
void arch_processor_init(struct processor *proc)
{
	x86_64_gdt_init(proc);
	x86_64_tss_init(proc);
	if(proc->flags & PROCESSOR_BSP) {
		proc->arch.kernel_stack = &initial_boot_stack;
	}

	/* save GS kernel base */
	uint64_t gs = (uint64_t)&proc->arch;
	x86_64_wrmsr(X86_MSR_GS_BASE, gs & 0xFFFFFFFF, gs >> 32);
	x86_64_wrmsr(X86_MSR_KERNEL_GS_BASE, gs & 0xFFFFFFFF, gs >> 32);

	/* okay, now set up the registers for fast syscall.
	 * This means storing x86_64_syscall_entry to LSTAR,
	 * the EFLAGS mask to SFMASK, and the CS kernel segment
	 * to STAR. */

	
	/* STAR: bits 32-47 are kernel CS, 48-63 are user CS. */
	uint32_t lo = 0, hi;
	hi = (0x1bull << 16) | 0x08;
	x86_64_wrmsr(X86_MSR_STAR, lo, hi);

	/* LSTAR: contains kernel entry point for syscall */
	lo = (uintptr_t)(&x86_64_syscall_entry_from_userspace) & 0xFFFFFFFF;
	hi = ((uintptr_t)(&x86_64_syscall_entry_from_userspace) >> 32) & 0xFFFFFFFF;
	x86_64_wrmsr(X86_MSR_LSTAR, lo, hi);

	/* SFMASK contains mask for eflags. Each bit set in SFMASK will
	 * be cleared in eflags on syscall */
	/*      TF         IF          DF        IOPL0       IOPL1         NT         AC */
	lo = (1 << 8) | (1 << 9) | (1 << 10) | (1 << 12) | (1 << 13) | (1 << 14) | (1 << 18);
	hi = 0;
	x86_64_wrmsr(X86_MSR_SFMASK, lo, hi);

	struct thread thread;
	asm("cli");
	thread.processor = proc;
	thread.arch.tcb.rcx = &user_test;
	x86_64_resume(&thread);
}

