#include <memory.h>
#include <arch/x86_64-msr.h>

#define VMCS_SIZE 0x1000
static uintptr_t vmcs;
static uint32_t revision_id;

static uint32_t __attribute__((noinline)) cpuid(uint32_t x, int rnum)
{
	uint32_t regs[4];
	asm volatile("push %%rbx; cpuid; mov %%ebx, %0; pop %%rbx" : "=a"(regs[0]), "=r"(regs[1]), "=c"(regs[2]), "=d"(regs[3]) : "a"(x));
	return regs[rnum];
}

static void x86_64_enable_vmx(void)
{
	uint32_t ecx = cpuid(1, 2);
	if(!(ecx & (1 << 5))) {
		panic("VMX extensions not available (not supported");
	}

	uint32_t lo, hi;
	x86_64_rdmsr(X86_MSR_FEATURE_CONTROL, &lo, &hi);
	if(!(lo & 1) /* lock bit */ || !(lo & (1 << 2)) /* enable-outside-smx bit */) {
		panic("VMX extensions not available (not enabled)");
	}

	/* okay, now try to enable VMX instructions. */
	uint64_t cr4;
	asm volatile("mov %%cr4, %0" : "=r"(cr4));
	cr4 |= (1 << 13); //enable VMX
	uintptr_t vmxon_region = mm_physical_alloc(0x1000, PM_TYPE_DRAM, true);
	
	x86_64_rdmsr(X86_MSR_VMX_BASIC, &lo, &hi);
	revision_id = lo & 0x7FFFFFFF;
	uint32_t *vmxon_revid = (uint32_t *)(vmxon_region + PHYSICAL_MAP_START);
	*vmxon_revid = revision_id;

	uint8_t error;
	asm volatile("mov %1, %%cr4; vmxon %2; setc %0" :"=g"(error): "r"(cr4), "m"(vmxon_region) : "cc");
	if(error) {
		panic("failed to enter VMX operation");
	}
	printk("VMX enabled.\n");
}

void x86_64_start_vmx(void)
{
	x86_64_enable_vmx();

	printk("Starting VMX system\n");
	vmcs = mm_physical_alloc(VMCS_SIZE, PM_TYPE_DRAM, true);
	printk(":: %lx\n", vmcs);
	uint32_t *vmcs_rev = (uint32_t *)(vmcs + PHYSICAL_MAP_START);
	*vmcs_rev = revision_id & 0x7FFFFFFF;

	uint8_t error;
	asm volatile("vmptrld (%%rax); setna %0" : "=g"(error) : "a"(&vmcs) : "cc");
	if(error) {
		panic("failed to load VMCS region");
	}

	printk("Got here.\n");
	for(;;);
}

