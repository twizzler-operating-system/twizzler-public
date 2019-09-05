/* We're going to assume, since we're x86_64, that the APIC exists. If
 * it doesn't, buy a better computer.
 */

#include <arch/x86_64-io.h>
#include <arch/x86_64-madt.h>
#include <arch/x86_64-msr.h>
#include <clksrc.h>
#include <debug.h>
#include <interrupt.h>
#include <memory.h>
#include <printk.h>
#include <processor.h>
#include <stdint.h>
#include <string.h>
#include <system.h>
#include <thread-bits.h>
static uintptr_t lapic_addr = 0;

#define LAPIC_DISABLE 0x10000
#define LAPIC_ID 0x20
#define LAPIC_VER 0x30
#define LAPIC_TPR 0x80
#define LAPIC_APR 0x90
#define LAPIC_PPR 0xA0
#define LAPIC_EOI 0xB0
#define LAPIC_LDR 0xD0
#define LAPIC_DFR 0xE0
#define LAPIC_SPIV 0xF0
#define LAPIC_SPIV_ENABLE_APIC 0x100
#define LAPIC_ISR 0x100
#define LAPIC_TMR 0x180
#define LAPIC_IRR 0x200
#define LAPIC_ESR 0x280
#define LAPIC_ICR 0x300
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
#define LAPIC_LVTT 0x320
#define LAPIC_LVTPC 0x340
#define LAPIC_LVT0 0x350
#define LAPIC_LVT1 0x360
#define LAPIC_LVTE 0x370
#define LAPIC_TICR 0x380
#define LAPIC_TCCR 0x390
#define LAPIC_TDCR 0x3E0

static inline void lapic_write(int reg, uint32_t data)
{
	asm volatile("mfence; lfence;" ::: "memory");
	*((volatile uint32_t *)(lapic_addr + reg)) = data;
}

__noinstrument static inline uint32_t lapic_read(int reg)
{
	return *(volatile uint32_t *)(lapic_addr + reg);
}

void x86_64_signal_eoi(void)
{
	lapic_write(LAPIC_EOI, 0x0);
}

__noinstrument unsigned int arch_processor_current_id(void)
{
	if(!lapic_addr)
		return 0;
	return lapic_read(LAPIC_ID) >> 24;
}

#define PIT_CHANNEL_BIT 6
#define PIT_ACCESS_BIT 4
#define PIT_MODE_BIT 1
#define PIT_FORMAT_BIT 0

#define PIT_CHANNEL(n) ((n) << PIT_CHANNEL_BIT)
#define PIT_READBACK PIT_CHANNEL(3)

#define PIT_ACCESS(n) ((n) << PIT_ACCESS_BIT)
#define PIT_ACCESS_LATCH PIT_ACCESS(0)
#define PIT_ACCESS_LO PIT_ACCESS(1)
#define PIT_ACCESS_HI PIT_ACCESS(2)
#define PIT_ACCESS_BOTH PIT_ACCESS(3)

#define PIT_MODE(n) ((n) << PIT_MODE_BIT)
#define PIT_MODE_IOTC PIT_MODE(0)
#define PIT_MODE_ONESHOT PIT_MODE(1)
#define PIT_MODE_RATEGEN PIT_MODE(2)
#define PIT_MODE_SQUARE PIT_MODE(3)
#define PIT_MODE_SOFTSTROBE PIT_MODE(4)
#define PIT_MODE_HARDSTROBE PIT_MODE(5)
#define PIT_MODE_RATE2 PIT_MODE(6)
#define PIT_MODE_SQUARE2 PIT_MODE(7)

#define PIT_FORMAT(n) ((n) << PIT_FORMAT_BIT)
#define PIT_FORMAT_BINARY PIT_FORMAT(0)
#define PIT_FORMAT_BCD PIT_FORMAT(1)
#define PIT_BASE 0x40
#define PIT_CMD (PIT_BASE + 3)
#define PIT_DATA(channel) (PIT_BASE + (channel))

static uint64_t wait_ns(int64_t ns)
{
	__int128 x = ns;
	x *= 1193182ul;
	int64_t count = (int64_t)(x / 1000000000ul);

	x86_64_outb(PIT_CMD, PIT_CHANNEL(2) | PIT_ACCESS_BOTH | PIT_MODE_ONESHOT | PIT_FORMAT_BINARY);

	uint64_t ec = 0;
	while(count > 64) {
		uint32_t readback;
		uint32_t thiscount = 0xFFFF;
		if(thiscount > count) {
			thiscount = count + 64;
			if(thiscount > 0xFFFF)
				thiscount = 0xFFFF;
		}

		x86_64_outb(PIT_DATA(2), thiscount & 0xFF);
		x86_64_outb(PIT_DATA(2), (thiscount >> 8) & 0xFF);
		/* force count to be reloaded */
		x86_64_outb(0x61, 0);
		x86_64_outb(0x61, 1);

		do {
			x86_64_outb(PIT_CMD, PIT_CHANNEL(2) | PIT_ACCESS_LATCH);
			readback = x86_64_inb(PIT_DATA(2));
			readback |= x86_64_inb(PIT_DATA(2)) << 8;
			asm("pause");
		} while(readback > 64);

		ec += (thiscount - readback);
		count -= (thiscount - readback);
	}
	x = ec;
	x *= 1000000000ul;
	return x / 1193182ul;
}

#define APIC_TIMER_DIV 1

static void _apic_set_counter(struct clksrc *c __unused, uint64_t val, bool per)
{
	lapic_write(LAPIC_LVTT, 32 | (per ? (1 << 17) : 0));
	lapic_write(LAPIC_TICR, val);
}

static uint64_t _apic_read_counter(struct clksrc *c __unused)
{
	return lapic_read(LAPIC_TCCR);
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
		lapic_write(LAPIC_LVTT, 32 | 0x20000);
		lapic_write(LAPIC_TDCR, div);
		lapic_write(LAPIC_TICR, 1000000000);

		uint64_t lts = lapic_read(LAPIC_TCCR);
		uint64_t rs = rdtsc();

		uint64_t elap = wait_ns(20000000);

		uint64_t re = rdtsc();
		uint64_t lte = lapic_read(LAPIC_TCCR);

		__int128 x = -(lte - lts);
		x *= 1000000000ul;
		uint64_t lt_freq = x / elap;
		x = re - rs;
		x *= 1000000000ul;
		uint64_t tc_freq = x / elap;

		uint64_t this_lapic_period_ps = 1000000000000ul / lt_freq;
		uint64_t this_tsc_period_ps = 1000000000000ul / tc_freq;

		lapic_write(LAPIC_LVTT, 32);
		lapic_write(LAPIC_TDCR, div);
		lapic_write(LAPIC_TICR, (1000000000) / this_lapic_period_ps);

		rs = rdtsc();
		while(lapic_read(LAPIC_TCCR) > 0)
			asm("pause");
		re = rdtsc();

		int64_t quality = 1000 - ((re - rs) * this_tsc_period_ps) / 1000000;
		if(quality < 0)
			quality = -quality;

		if(attempts == 0)
			quality = 1000; /* throw away first try */

		if(quality < calib_qual) {
			calib_qual = quality;
			lapic_period_ps = this_lapic_period_ps;
			tsc_period_ps = this_tsc_period_ps;
		}

		if(quality == 0)
			break;
	}

	_clksrc_apic.period_ps = lapic_period_ps;
	_clksrc_tsc.period_ps = tsc_period_ps;

	uint64_t diff = 0;
	uint64_t i;
	for(i = 0; i < 100; i++) {
		lapic_write(LAPIC_TICR, (1000ul * 1000000ul) / lapic_period_ps);
		uint64_t s = lapic_read(LAPIC_TCCR);
		uint64_t e = lapic_read(LAPIC_TCCR);
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
		lapic_read(LAPIC_TCCR);
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
	/* these are not yet configured */
	lapic_write(LAPIC_DFR, 0xFFFFFFFF);
	lapic_write(LAPIC_LDR, (lapic_read(LAPIC_LDR) & 0x00FFFFFF) | 1);
	/* disable the timer while we set up */
	lapic_write(LAPIC_LVTT, LAPIC_DISABLE);

	/* if we accept the extint stuff (the boot processor) we need to not
	 * mask, and set the proper flags for these entries.
	 * LVT1: NMI
	 * LVT0: extINT, level triggered
	 */
	lapic_write(LAPIC_LVT1, 0x400 | (bsp ? 0 : LAPIC_DISABLE));  // NMI
	lapic_write(LAPIC_LVT0, 0x8700 | (bsp ? 0 : LAPIC_DISABLE)); // external interrupts
	/* disable errors (can trigger while messing with masking) and performance
	 * counter, but also set a non-zero vector */
	lapic_write(LAPIC_LVTE, 0xFF | LAPIC_DISABLE);
	lapic_write(LAPIC_LVTPC, 0xFF | LAPIC_DISABLE);

	/* accept all priority levels */
	lapic_write(LAPIC_TPR, 0);
	/* finally write to the spurious interrupt register to enable
	 * the interrupts */
	lapic_write(LAPIC_ESR, 0);
	lapic_write(LAPIC_SPIV, 0x0100 | 0xFF);
	int div = APIC_TIMER_DIV;
	if(bsp)
		calibrate_timers(div);

	lapic_write(LAPIC_LVTT, 32);
	lapic_write(LAPIC_TDCR, div);
	lapic_write(LAPIC_TICR, 0);
}

__initializer void x86_64_lapic_init_percpu(void)
{
	/* enable the LAPIC */
	uint32_t lo, hi;
	x86_64_rdmsr(X86_MSR_APIC_BASE, &lo, &hi);
	uintptr_t paddr = ((((uintptr_t)hi & 0xF) << 32) | lo) & ~0xFFF;

	lo |= X86_MSR_APIC_BASE_ENABLE;
	x86_64_wrmsr(X86_MSR_APIC_BASE, lo, hi);

	lapic_addr = (uintptr_t)mm_ptov(paddr);
	lapic_configure(lo & X86_MSR_APIC_BASE_BSP);
}

static void x86_64_apic_send_ipi(unsigned char dest_shorthand, unsigned int dst, unsigned int v)
{
	assert((v & LAPIC_ICR_DM_INIT) || (v & LAPIC_ICR_LEVELASSERT));
	int send_status;
	/* Writing to the lower ICR register causes the interrupt
	 * to get sent off (Intel 3A 10.6.1), so do the higher reg first */
	lapic_write(LAPIC_ICR + 0x10, (dst << 24));
	unsigned lower = v | (dest_shorthand << 18);
	/* gotta have assert for all except init */
	lapic_write(LAPIC_ICR, lower);
	/* Wait for send to finish */
	while((send_status = lapic_read(LAPIC_ICR)) & LAPIC_ICR_STATUS_PEND)
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
	proc->arch.kernel_stack = (void *)mm_virtual_alloc(KERNEL_STACK_SIZE, PM_TYPE_ANY, true);
	printk("Poking secondary CPU %d, proc->arch.kernel_stack = %p (proc=%p)\n",
	  proc->id,
	  proc->arch.kernel_stack,
	  proc);
	*(void **)(proc->arch.kernel_stack + KERNEL_STACK_SIZE - sizeof(void *)) = proc;

	uintptr_t bootaddr_phys = 0x7000;

	lapic_write(LAPIC_ESR, 0);
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
	lapic_write(LAPIC_ESR, 0);
	x86_64_apic_send_ipi(LAPIC_ICR_SHORT_DEST,
	  proc->id,
	  LAPIC_ICR_TM_LEVEL | LAPIC_ICR_LEVELASSERT | LAPIC_ICR_DM_INIT);

	wait_ns(100000);
	x86_64_apic_send_ipi(LAPIC_ICR_SHORT_DEST, proc->id, LAPIC_ICR_TM_LEVEL | LAPIC_ICR_DM_INIT);

	wait_ns(100000);
	for(int i = 0; i < 3; i++) {
		x86_64_apic_send_ipi(
		  LAPIC_ICR_SHORT_DEST, proc->id, LAPIC_ICR_DM_SIPI | ((bootaddr_phys >> 12) & 0xFF));
		wait_ns(10000);
	}

	int timeout;
	for(timeout = 10000; timeout > 0; timeout--) {
		if(*((volatile _Atomic uintptr_t *)mm_ptov(0x7300)) == 0)
			break;
		wait_ns(10000);
	}

	if(timeout == 0) {
		printk("failed to start CPU %d\n", proc->id);
	}
}
