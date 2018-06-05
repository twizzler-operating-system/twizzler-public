/* We're going to assume, since we're x86_64, that the APIC exists. If
 * it doesn't, buy a better computer.
 */

#include <stdint.h>
#include <arch/x86_64-msr.h>
#include <system.h>
#include <printk.h>
#include <string.h>
#include <memory.h>
#include <debug.h>
#include <thread-bits.h>
#include <processor.h>
#include <arch/x86_64-madt.h>
#include <interrupt.h>
#include <arch/x86_64-io.h>
static uintptr_t lapic_addr = 0;
struct spinlock ipi_lock = SPINLOCK_INIT;

#define LAPIC_DISABLE          0x10000
#define LAPIC_ID                                0x20
#define LAPIC_VER                               0x30
#define LAPIC_TPR                               0x80
#define LAPIC_APR                               0x90
#define LAPIC_PPR                               0xA0
#define LAPIC_EOI                               0xB0
#define LAPIC_LDR                               0xD0
#define LAPIC_DFR                               0xE0
#define LAPIC_SPIV                              0xF0
#define LAPIC_SPIV_ENABLE_APIC  0x100
#define LAPIC_ISR                               0x100
#define LAPIC_TMR                               0x180
#define LAPIC_IRR                               0x200
#define LAPIC_ESR                               0x280
#define LAPIC_ICR                               0x300
#define LAPIC_ICR_DS_SELF               0x40000
#define LAPIC_ICR_DS_ALLINC             0x80000
#define LAPIC_ICR_DS_ALLEX              0xC0000
#define LAPIC_ICR_TM_LEVEL              0x8000
#define LAPIC_ICR_LEVELASSERT   0x4000
#define LAPIC_ICR_STATUS_PEND   0x1000
#define LAPIC_ICR_DM_LOGICAL    0x800
#define LAPIC_ICR_DM_LOWPRI             0x100
#define LAPIC_ICR_DM_SMI                0x200
#define LAPIC_ICR_DM_NMI                0x400
#define LAPIC_ICR_DM_INIT               0x500
#define LAPIC_ICR_DM_SIPI               0x600
#define LAPIC_ICR_SHORT_DEST    0x0
#define LAPIC_ICR_SHORT_SELF    0x1
#define LAPIC_ICR_SHORT_ALL     0x2
#define LAPIC_ICR_SHORT_OTHERS  0x3
#define LAPIC_LVTT                              0x320
#define LAPIC_LVTPC                     0x340
#define LAPIC_LVT0                              0x350
#define LAPIC_LVT1                              0x360
#define LAPIC_LVTE                              0x370
#define LAPIC_TICR                              0x380
#define LAPIC_TCCR                              0x390
#define LAPIC_TDCR                              0x3E0

static inline void lapic_write(int reg, uint32_t data)
{
    *((volatile uint32_t *)(lapic_addr + reg)) = data;
}

__noinstrument
static inline uint32_t lapic_read(int reg)
{
	return *(volatile uint32_t *)(lapic_addr + reg);
}

void x86_64_signal_eoi(void)
{
    lapic_write(LAPIC_EOI, 0x0);
}

__noinstrument
unsigned int arch_processor_current_id(void)
{
	if(!lapic_addr)
		return 0;
	return lapic_read(LAPIC_ID) >> 24;
}


#define PIT_CHANNEL_BIT     6
#define PIT_ACCESS_BIT      4
#define PIT_MODE_BIT        1
#define PIT_FORMAT_BIT      0

#define PIT_CHANNEL(n)      ((n) << PIT_CHANNEL_BIT)
#define PIT_READBACK        PIT_CHANNEL(3)

#define PIT_ACCESS(n)       ((n) << PIT_ACCESS_BIT)
#define PIT_ACCESS_LATCH    PIT_ACCESS(0)
#define PIT_ACCESS_LO       PIT_ACCESS(1)
#define PIT_ACCESS_HI       PIT_ACCESS(2)
#define PIT_ACCESS_BOTH     PIT_ACCESS(3)

#define PIT_MODE(n)         ((n) << PIT_MODE_BIT)
#define PIT_MODE_IOTC       PIT_MODE(0)
#define PIT_MODE_ONESHOT    PIT_MODE(1)
#define PIT_MODE_RATEGEN    PIT_MODE(2)
#define PIT_MODE_SQUARE     PIT_MODE(3)
#define PIT_MODE_SOFTSTROBE PIT_MODE(4)
#define PIT_MODE_HARDSTROBE PIT_MODE(5)
#define PIT_MODE_RATE2      PIT_MODE(6)
#define PIT_MODE_SQUARE2    PIT_MODE(7)

#define PIT_FORMAT(n)       ((n) << PIT_FORMAT_BIT)
#define PIT_FORMAT_BINARY   PIT_FORMAT(0)
#define PIT_FORMAT_BCD      PIT_FORMAT(1)
#define PIT_BASE            0x40
#define PIT_CMD             (PIT_BASE + 3)
#define PIT_DATA(channel) (PIT_BASE + (channel))

static uint32_t do_wait(int ns)
{
	__int128 x = ns * 1193182ul;
	uint64_t count = x / 1000000000ul;
	count += 64;
	if(count > 0xffff) count = 0xffff;
	//printk(":: %lx\n", count);
	//uint64_t count = mul_64_64_div_64(nanosec, 1193182, 1000000000);
	x86_64_outb(PIT_CMD, PIT_CHANNEL(2) | PIT_ACCESS_BOTH |
			PIT_MODE_ONESHOT | PIT_FORMAT_BINARY);
	x86_64_outb(PIT_DATA(2), count & 0xFF);
	x86_64_outb(PIT_DATA(2), (count >> 8) & 0xFF);

	uint32_t readback;
	do {
		x86_64_outb(PIT_CMD,
				PIT_CHANNEL(2) |
				PIT_ACCESS_LATCH);
		readback = x86_64_inb(PIT_DATA(2));
		readback |= x86_64_inb(PIT_DATA(2)) << 8;
		asm("pause");
	} while (readback > 64);
	x = (count - readback) * 1000000000ul;
	return x / 1193182;
}

static uint64_t wait_ns(int64_t ns)
{
	uint64_t el = 0;
	while(ns > 100) {
		uint64_t tmp = do_wait(ns);
		ns -= tmp;
		el += tmp;
	}
	return el;
}

static uint64_t lapic_period_ps;
static uint64_t tsc_period_ps;
static int calib_qual = 0;

uint64_t arch_processor_get_nanoseconds(void)
{
	return (tsc_period_ps * rdtsc()) / 1000;
}

static void set_lapic_timer(unsigned ns)
{
	calib_qual = 1000;
    /* THE ORDER OF THESE IS IMPORTANT!
     * If you write the initial count before you
     * tell it to set periodic mode and set the vector
     * it will not generate an interrupt! */
	int attempts;
	for(attempts = 0;attempts < 10;attempts++) {
		lapic_write(LAPIC_LVTT, 32 | 0x20000);
		lapic_write(LAPIC_TDCR, 3);
		lapic_write(LAPIC_TICR, 1000000000);

		uint64_t lts = lapic_read(LAPIC_TCCR);
		uint64_t rs = rdtsc();

		uint64_t elap = wait_ns(10000000);

		uint64_t re = rdtsc();
		uint64_t lte = lapic_read(LAPIC_TCCR);

		uint64_t lt_freq = (-(lte - lts) * 1000000000) / elap;
		uint64_t tc_freq = ((re - rs) * 1000000000) / elap;

		uint64_t this_lapic_period_ps = 1000000000000ul / lt_freq;
		uint64_t this_tsc_period_ps   = 1000000000000ul / tc_freq;

		lapic_write(LAPIC_LVTT, 32);
		lapic_write(LAPIC_TDCR, 3);
		lapic_write(LAPIC_TICR, (1000ul * ns) / this_lapic_period_ps);

		rs = rdtsc();
		while(lapic_read(LAPIC_TCCR) > 0);
		re = rdtsc();

		int quality = 1000 - ((re - rs) * this_tsc_period_ps) / ns;
		if(quality < 0) quality = -quality;

		if(attempts == 0) quality = 1000; /* throw away first try */

		if(quality < calib_qual) {
			calib_qual = quality;
			lapic_period_ps = this_lapic_period_ps;
			tsc_period_ps = this_tsc_period_ps;
		}

		if(quality == 0) break;
	}
	printk("Calibrated lapic timer (%ld ps) and TSC (%ld ps) after %d attempts (q=%d)\n", lapic_period_ps, tsc_period_ps, attempts, calib_qual);
}

static void lapic_configure(int bsp)
{
	for(int i=0;i<=255;i++)
		x86_64_signal_eoi();
	/* these are not yet configured */
	lapic_write(LAPIC_DFR, 0xFFFFFFFF);
	lapic_write(LAPIC_LDR, (lapic_read(LAPIC_LDR)&0x00FFFFFF)|1);
	/* disable the timer while we set up */
	lapic_write(LAPIC_LVTT, LAPIC_DISABLE);

    /* if we accept the extint stuff (the boot processor) we need to not
     * mask, and set the proper flags for these entries.
     * LVT1: NMI
     * LVT0: extINT, level triggered
     */
    lapic_write(LAPIC_LVT1, 0x400 | (bsp ? 0 : LAPIC_DISABLE)); //NMI
    lapic_write(LAPIC_LVT0, 0x8700 | (bsp ? 0 : LAPIC_DISABLE)); //external interrupts
    /* disable errors (can trigger while messing with masking) and performance
     * counter, but also set a non-zero vector */
    lapic_write(LAPIC_LVTE, 0xFF | LAPIC_DISABLE);
    lapic_write(LAPIC_LVTPC, 0xFF | LAPIC_DISABLE);

    /* accept all priority levels */
    lapic_write(LAPIC_TPR, 0);
    /* finally write to the spurious interrupt register to enable
     * the interrupts */
    lapic_write(LAPIC_SPIV, 0x0100 | 0xFF);
	set_lapic_timer(1000000);
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

/* TODO: need to make this better (locks expensive?) */
static void x86_cpu_send_ipi(unsigned char dest_shorthand, unsigned int dst, unsigned int v)
{
    assert((v & LAPIC_ICR_DM_INIT) || (v & LAPIC_ICR_LEVELASSERT));
    int send_status;
    int old = arch_interrupt_set(0);
    bool fl = spinlock_acquire(&ipi_lock);
    /* Writing to the lower ICR register causes the interrupt
     * to get sent off (Intel 3A 10.6.1), so do the higher reg first */
    lapic_write(LAPIC_ICR+0x10, (dst << 24));
    unsigned lower = v | (dest_shorthand << 18);
    /* gotta have assert for all except init */
    lapic_write(LAPIC_ICR, lower);
    /* Wait for send to finish */
    do {
        asm("pause");
        send_status = lapic_read(LAPIC_ICR) & LAPIC_ICR_STATUS_PEND;
    } while (send_status);
    spinlock_release(&ipi_lock, fl);
    arch_interrupt_set(old);
}

void x86_64_processor_send_ipi(long dest, int signal)
{
	x86_cpu_send_ipi(dest == PROCESSOR_IPI_DEST_OTHERS
				? LAPIC_ICR_SHORT_OTHERS : LAPIC_ICR_SHORT_DEST,
			dest == PROCESSOR_IPI_DEST_OTHERS ? 0 : dest,
			LAPIC_ICR_LEVELASSERT | LAPIC_ICR_TM_LEVEL | signal);
}

void x86_64_processor_halt_others(void)
{
	x86_64_processor_send_ipi(PROCESSOR_IPI_DEST_OTHERS, PROCESSOR_IPI_HALT);
}

void arch_panic_begin(void)
{
	x86_64_processor_halt_others();
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
static inline void write_bios_reset(uintptr_t addr)
{
	*((volatile uint32_t *)mm_ptov(BIOS_RESET_VECTOR)) = ((addr & 0xFF000) << 12);
}

extern int trampoline_start, trampoline_end, rm_gdt, pmode_enter, rm_gdt_pointer;
void arch_processor_boot(struct processor *proc)
{
	proc->arch.kernel_stack = (void *)mm_virtual_alloc(KERNEL_STACK_SIZE, PM_TYPE_ANY, true);
	printk("Poking secondary CPU %d, proc->arch.kernel_stack = %p (proc=%p)\n", proc->id, proc->arch.kernel_stack, proc);
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

	*((volatile uintptr_t *)mm_ptov(0x7300)) = (uintptr_t)proc->arch.kernel_stack + KERNEL_STACK_SIZE;
	asm volatile("mfence" ::: "memory");
	lapic_write(LAPIC_ESR, 0);
	x86_cpu_send_ipi(LAPIC_ICR_SHORT_DEST, proc->id, LAPIC_ICR_TM_LEVEL | LAPIC_ICR_LEVELASSERT | LAPIC_ICR_DM_INIT);

	wait_ns(100000);
	x86_cpu_send_ipi(LAPIC_ICR_SHORT_DEST, proc->id, LAPIC_ICR_TM_LEVEL | LAPIC_ICR_DM_INIT);

	wait_ns(100000);
	for(int i=0;i<3;i++) {
		x86_cpu_send_ipi(LAPIC_ICR_SHORT_DEST, proc->id, LAPIC_ICR_DM_SIPI | ((bootaddr_phys >> 12) & 0xFF));
		wait_ns(10000);
	}

	int timeout;
	for(timeout=10000;timeout>0;timeout--) {
		if(*(volatile _Atomic uintptr_t *)(0x7300) == 0)
			break;
		wait_ns(10000);
	}

	if(timeout == 0) {
		printk("failed to start CPU %d\n", proc->id);
	}
}

