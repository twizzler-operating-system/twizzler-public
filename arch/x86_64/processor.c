#include <arch/x86_64-msr.h>
#include <processor.h>

void arch_processor_enumerate()
{
	/* this is handled by initializers in madt.c */
}

static _Alignas(16) struct {
	uint16_t limit;
	uint64_t base;
} __attribute__((packed)) idt_ptr;

void arch_processor_reset(void)
{
	idt_ptr.limit = 0;
	asm volatile("lidt (%0)" ::"r"(&idt_ptr) : "memory");
	asm volatile("int $3");
}

#define MSR_IA32_TME_ACTIVATE 0x982
size_t arch_processor_physical_width(void)
{
	static size_t physical_address_bits = 0;
	if(!physical_address_bits) {
		uint32_t a, b, c, d;
		if(!__get_cpuid_count(0x80000008, 0, &a, &b, &c, &d))
			panic("unable to determine physical address size");
		physical_address_bits = a & 0xff;
	}
	return physical_address_bits - 1;
}

size_t arch_processor_virtual_width(void)
{
	static size_t virtual_address_bits = 0;
	if(!virtual_address_bits) {
		uint32_t a, b, c, d;
		if(!__get_cpuid_count(0x80000008, 0, &a, &b, &c, &d))
			panic("unable to determine physical address size");
		virtual_address_bits = (a >> 8) & 0xff;
	}
	return virtual_address_bits - 1;
}

void arch_processor_scheduler_wakeup(struct processor *proc)
{
	proc->flags |= PROCESSOR_HASWORK;
	if(!(x86_features.features & X86_FEATURE_MWAIT)) {
		/* if we don't have mwait, the sleeping processor is in HLT loop, which means we must send
		 * it an IPI to wake it. */
		/* TODO: check if processor is halted */
		processor_send_ipi(proc->id, PROCESSOR_IPI_RESUME, NULL, PROCESSOR_IPI_NOWAIT);
	}
}

__noinstrument void arch_processor_halt(struct processor *proc)
{
	if(x86_features.features & X86_FEATURE_MWAIT) {
		if(proc->flags & PROCESSOR_HASWORK)
			goto wakeup;

		asm volatile("mfence; clflush 0(%0); mfence" ::"r"(&proc->flags) : "memory");
		asm volatile("monitor; mfence" ::"a"(&proc->flags), "d"(0ul), "c"(0ul) : "memory");

		if(proc->flags & PROCESSOR_HASWORK)
			goto wakeup;

		asm volatile("mwait" ::"a"(0x20), "c"(0x1) : "memory");
	} else {
		/* note the race-condition: HASWORK is checked before the halt and not after. this is okay;
		 * if we fall-back to the halt loop, we also use the RESUME IPI. */
		asm volatile("sti; hlt");
	}

wakeup:
	proc->flags &= ~PROCESSOR_HASWORK;

	// asm volatile("sti; hlt");
	// for(long i = 0; i < 10000000; i++) {
	//	asm volatile("pause");
	//}
}
