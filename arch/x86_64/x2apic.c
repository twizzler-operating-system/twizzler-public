#include <arch/x86_64-io.h>
#include <arch/x86_64-msr.h>
#include <arch/x86_64.h>
#include <clksrc.h>
#include <debug.h>
#include <interrupt.h>
#include <memory.h>
#include <processor.h>
#include <thread-bits.h>

#define LAPIC_DISABLE 0x10000
#define LAPIC_ID 0x802
#define LAPIC_VER 0x803
#define LAPIC_TPR 0x808
#define LAPIC_PPR 0x80A
#define LAPIC_EOI 0x80B
#define LAPIC_LDR 0x80D
#define LAPIC_SPIV 0x80F
#define LAPIC_SPIV_ENABLE_APIC 0x100
#define LAPIC_ISR_BASE 0x810
#define LAPIC_TMR_BASE 0x818
#define LAPIC_IRR_BASE 0x820
#define LAPIC_ESR 0x828
#define LAPIC_ICR 0x830
#define LAPIC_ICR_DS_SELF 0x40000
#define LAPIC_ICR_DS_ALLINC 0x80000
#define LAPIC_ICR_DS_ALLEX 0xC0000
#define LAPIC_ICR_TM_LEVEL 0x8000
#define LAPIC_ICR_LEVELASSERT 0x4000
#define LAPIC_ICR_STATUS_PEND 0x1000
#define LAPIC_ICR_DM_LOGICAL 0x800
#define LAPIC_ICR_DM_LOWPRI 0x100
#define LAPIC_ICR_DM_SMI 0x200
#define LAPIC_ICR_DM_NMI 0x400
#define LAPIC_ICR_DM_INIT 0x500
#define LAPIC_ICR_DM_SIPI 0x600
#define LAPIC_ICR_SHORT_DEST 0x0
#define LAPIC_ICR_SHORT_SELF 0x1
#define LAPIC_ICR_SHORT_ALL 0x2
#define LAPIC_ICR_SHORT_OTHERS 0x3
#define LAPIC_LVTT 0x832
#define LAPIC_LVTPC 0x834
#define LAPIC_LVT0 0x835
#define LAPIC_LVT1 0x836
#define LAPIC_LVTE 0x837
#define LAPIC_TICR 0x838
#define LAPIC_TCCR 0x839
#define LAPIC_TDCR 0x83E

static inline void x2apic_write(uint32_t reg, uint64_t data)
{
	uint32_t lo = (uint32_t)data;
	uint32_t hi = (uint32_t)(data >> 32);
	x86_64_wrmsr(reg, lo, hi);
}

static inline uint64_t x2apic_read(uint32_t reg)
{
	uint32_t lo, hi;
	x86_64_rdmsr(reg, &lo, &hi);
	return ((uint64_t)hi << 32) | (uint64_t)lo;
}

void x86_64_signal_eoi(void)
{
	x2apic_write(LAPIC_EOI, 0x0);
}

__noinstrument unsigned int arch_processor_current_id(void)
{
	/* TODO: is this right? */
	// return x2apic_read(LAPIC_LDR);
	return x2apic_read(LAPIC_ID);
}

static void _apic_set_counter(struct clksrc *c __unused, uint64_t val, bool per)
{
	x2apic_write(LAPIC_LVTT, 32 | (per ? (1 << 17) : 0));
	x2apic_write(LAPIC_TICR, val);
}

static uint64_t _apic_read_counter(struct clksrc *c __unused)
{
	return x2apic_read(LAPIC_TCCR);
}

__noinstrument static uint64_t _tsc_read_counter(struct clksrc *c __unused)
{
	uint32_t eax, edx;
	asm volatile("rdtscp; lfence" : "=a"(eax), "=d"(edx)::"memory", "rcx");
	return (uint64_t)eax | (uint64_t)edx << 32;
}

static inline uint64_t rdtsc(void)
{
	uint32_t eax, edx;
	asm volatile("rdtscp" : "=a"(eax), "=d"(edx)::"rcx");
	return (uint64_t)eax | (uint64_t)edx << 32;
}

static struct clksrc _clksrc_apic = {
	.name = "APIC Timer",
	.flags = CLKSRC_INTERRUPT | CLKSRC_ONESHOT | CLKSRC_PERIODIC,
	.read_counter = _apic_read_counter,
	.set_timer = _apic_set_counter,
};

static struct clksrc _clksrc_tsc = {
	.name = "TSC",
	.flags = CLKSRC_MONOTONIC,
	.read_counter = _tsc_read_counter,
};

void arch_syscall_kconf_set_tsc_period(long ps);
static void calibrate_timers(int div)
{
	uint64_t lapic_period_ps = 0;
	uint64_t tsc_period_ps = 0;
	int calib_qual = 0;
	arch_interrupt_set(false);
	calib_qual = 1000;
	int attempts;
	for(attempts = 0; attempts < 5; attempts++) {
		/* THE ORDER OF THESE IS IMPORTANT!
		 * If you write the initial count before you
		 * tell it to set periodic mode and set the vector
		 * it will not generate an interrupt! */
		x2apic_write(LAPIC_TDCR, div);
		x2apic_write(LAPIC_LVTT, 32 | 0x20000);
		x2apic_write(LAPIC_TICR, 1000000000);

		uint64_t lts = x2apic_read(LAPIC_TCCR);
		uint64_t rs = rdtsc();

		uint64_t elap = x86_64_early_wait_ns(20000000);

		uint64_t re = rdtsc();
		uint64_t lte = x2apic_read(LAPIC_TCCR);

		__int128 x = -(lte - lts);
		x *= 1000000000ul;
		uint64_t lt_freq = x / elap;
		x = re - rs;
		x *= 1000000000ul;
		uint64_t tc_freq = x / elap;

		uint64_t this_lapic_period_ps = 1000000000000ul / lt_freq;
		uint64_t this_tsc_period_ps = 1000000000000ul / tc_freq;

		x2apic_write(LAPIC_LVTT, 32 | 0x10000);
		x2apic_write(LAPIC_TDCR, div);
		x2apic_write(LAPIC_TICR, (1000000000) / this_lapic_period_ps);

		rs = rdtsc();
		while(x2apic_read(LAPIC_TCCR) > 0)
			asm("pause");
		re = rdtsc();

		int64_t quality = 1000 - ((re - rs) * this_tsc_period_ps) / 1000000;
		if(quality < 0)
			quality = -quality;

		if(attempts == 0)
			quality = 1000; /* throw away first try */

		if(quality < calib_qual || (attempts && !lapic_period_ps)) {
			calib_qual = quality;
			lapic_period_ps = this_lapic_period_ps;
			tsc_period_ps = this_tsc_period_ps;
		}

		if(quality == 0)
			break;
	}

	_clksrc_apic.period_ps = lapic_period_ps;
	_clksrc_tsc.period_ps = tsc_period_ps;

	arch_syscall_kconf_set_tsc_period(tsc_period_ps);

	uint64_t diff = 0;
	uint64_t i;
	for(i = 0; i < 100; i++) {
		x2apic_write(LAPIC_TICR, (1000ul * 1000000ul) / lapic_period_ps);
		uint64_t s = x2apic_read(LAPIC_TCCR);
		uint64_t e = x2apic_read(LAPIC_TCCR);
		diff += -(e - s);
	}
	diff /= i;
	diff *= lapic_period_ps;
	diff /= 1000;
	if(diff == 0)
		diff = 1;
	_clksrc_apic.precision = diff; /* lapic counts down */

	diff = 0;
	for(i = 0; i < 100; i++) {
		uint64_t s = rdtsc();
		uint64_t e = rdtsc();
		diff += e - s;
	}
	diff /= i;
	diff *= tsc_period_ps;
	diff /= 1000;
	if(diff == 0)
		diff = 1;
	_clksrc_tsc.precision = diff;

	uint64_t s = rdtsc();
	for(i = 0; i < 1000; i++) {
		rdtsc();
	}
	uint64_t e = rdtsc();

	_clksrc_tsc.read_time = ((e - s) * tsc_period_ps) / (1000 * i);

	s = rdtsc();
	for(i = 0; i < 1000; i++) {
		x2apic_read(LAPIC_TCCR);
	}
	e = rdtsc();

	_clksrc_apic.read_time = ((e - s) * tsc_period_ps) / (1000 * i);

	clksrc_register(&_clksrc_apic);
	clksrc_register(&_clksrc_tsc);
}

static void lapic_configure(int bsp)
{
	for(int i = 0; i <= 255; i++)
		x86_64_signal_eoi();
	/* disable the timer while we set up */
	x2apic_write(LAPIC_LVTT, LAPIC_DISABLE);

	/* if we accept the extint stuff (the boot processor) we need to not
	 * mask, and set the proper flags for these entries.
	 * LVT1: NMI
	 * LVT0: extINT, level triggered
	 */
	x2apic_write(LAPIC_LVT1, 0x400 | (bsp ? 0 : LAPIC_DISABLE));  // NMI
	x2apic_write(LAPIC_LVT0, 0x8700 | (bsp ? 0 : LAPIC_DISABLE)); // external interrupts
	/* disable errors (can trigger while messing with masking) and performance
	 * counter, but also set a non-zero vector */
	x2apic_write(LAPIC_LVTE, 0xFF | LAPIC_DISABLE);
	x2apic_write(LAPIC_LVTPC, 0xFF | LAPIC_DISABLE);

	/* accept all priority levels */
	x2apic_write(LAPIC_TPR, 0);
	/* finally write to the spurious interrupt register to enable
	 * the interrupts */
	x2apic_write(LAPIC_ESR, 0);
	x2apic_write(LAPIC_SPIV, 0x0100 | 0xFF);
	int div = 1;
	if(bsp)
		calibrate_timers(div);

	x2apic_write(LAPIC_TDCR, div);
	x2apic_write(LAPIC_LVTT, 32);
	x2apic_write(LAPIC_TICR, 0);
}

void x86_64_lapic_init_percpu(void)
{
	uint32_t ecx = x86_64_cpuid(1, 0, 2);
	if(!(ecx & (1 << 21))) {
		panic("no support for x2APIC mode");
	}
	/* enable the LAPIC */
	uint32_t lo, hi;
	x86_64_rdmsr(X86_MSR_APIC_BASE, &lo, &hi);
	lo |= X86_MSR_APIC_BASE_ENABLE | X86_MSR_APIC_BASE_X2MODE;
	x86_64_wrmsr(X86_MSR_APIC_BASE, lo, hi);
	lapic_configure(lo & X86_MSR_APIC_BASE_BSP);
}

static void x86_64_apic_send_ipi(unsigned char dest_shorthand, unsigned int dst, unsigned int v)
{
	assert((v & LAPIC_ICR_DM_INIT) || (v & LAPIC_ICR_LEVELASSERT));
	uint64_t send_status;
	uint32_t icr_hi = dst;
	uint32_t icr_lo = v | (dest_shorthand << 18);
	uint64_t icr = ((uint64_t)icr_hi << 32) | (uint64_t)icr_lo;
	x2apic_write(LAPIC_ICR, icr);
	/* Wait for send to finish */
	while((send_status = x2apic_read(LAPIC_ICR)) & LAPIC_ICR_STATUS_PEND)
		asm("pause");
}

void arch_processor_send_ipi(int destid, int vector, int flags __unused)
{
	x86_64_apic_send_ipi(
	  destid == PROCESSOR_IPI_DEST_OTHERS ? LAPIC_ICR_SHORT_OTHERS : LAPIC_ICR_SHORT_DEST,
	  destid == PROCESSOR_IPI_DEST_OTHERS ? 0 : destid,
	  LAPIC_ICR_LEVELASSERT | LAPIC_ICR_TM_LEVEL | vector);
}

#define BIOS_RESET_VECTOR 0x467
#define CMOS_RESET_CODE 0xF
#define CMOS_RESET_JUMP 0xa
#define RM_GDT_SIZE 0x18
#define GDT_POINTER_SIZE 0x6
#define RM_GDT_START 0x7100
#define BOOTFLAG_ADDR 0x7200

#ifdef __clang__
__attribute__((no_sanitize("alignment")))
#else
__attribute__((no_sanitize_undefined))
#endif
static inline void
write_bios_reset(uintptr_t addr)
{
	*((volatile uint32_t *)mm_ptov(BIOS_RESET_VECTOR)) = ((addr & 0xFF000) << 12);
}

extern int trampoline_start, trampoline_end, rm_gdt, pmode_enter, rm_gdt_pointer;
void arch_processor_boot(struct processor *proc)
{
	proc->arch.kernel_stack = (void *)mm_memory_alloc(KERNEL_STACK_SIZE, PM_TYPE_ANY, true);
	// printk("Poking secondary CPU %d, proc->arch.kernel_stack = %p (proc=%p)\n",
	//  proc->id,
	//  proc->arch.kernel_stack,
	//  proc);
	*(void **)(proc->arch.kernel_stack + KERNEL_STACK_SIZE - sizeof(void *)) = proc;

	uintptr_t bootaddr_phys = 0x7000;

	x2apic_write(LAPIC_ESR, 0);
	write_bios_reset(bootaddr_phys);
	x86_64_cmos_write(CMOS_RESET_CODE, CMOS_RESET_JUMP);

	size_t trampoline_size = (uintptr_t)&trampoline_end - (uintptr_t)&trampoline_start;
	memcpy(mm_ptov(0x7000), &trampoline_start, trampoline_size);
	memcpy(mm_ptov(RM_GDT_START + GDT_POINTER_SIZE), &rm_gdt, RM_GDT_SIZE);
	memcpy(mm_ptov(RM_GDT_START), &rm_gdt_pointer, GDT_POINTER_SIZE);
	memcpy(mm_ptov(0x7200), &pmode_enter, 0x100);

	*((volatile uintptr_t *)mm_ptov(0x7300)) =
	  (uintptr_t)proc->arch.kernel_stack + KERNEL_STACK_SIZE;
	asm volatile("mfence" ::: "memory");
	x2apic_write(LAPIC_ESR, 0);
	x86_64_apic_send_ipi(LAPIC_ICR_SHORT_DEST,
	  proc->id,
	  LAPIC_ICR_TM_LEVEL | LAPIC_ICR_LEVELASSERT | LAPIC_ICR_DM_INIT);

	x86_64_early_wait_ns(100000);
	x86_64_apic_send_ipi(LAPIC_ICR_SHORT_DEST, proc->id, LAPIC_ICR_TM_LEVEL | LAPIC_ICR_DM_INIT);

	x86_64_early_wait_ns(100000);
	for(int i = 0; i < 3; i++) {
		x86_64_apic_send_ipi(
		  LAPIC_ICR_SHORT_DEST, proc->id, LAPIC_ICR_DM_SIPI | ((bootaddr_phys >> 12) & 0xFF));
		x86_64_early_wait_ns(10000);
	}

	int timeout;
	for(timeout = 10000; timeout > 0; timeout--) {
		if(*((volatile _Atomic uintptr_t *)mm_ptov(0x7300)) == 0)
			break;
		x86_64_early_wait_ns(10000);
	}

	if(timeout == 0) {
		printk("failed to start CPU %d\n", proc->id);
	}
}
