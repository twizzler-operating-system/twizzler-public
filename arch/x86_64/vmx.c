#include <memory.h>
#include <arch/x86_64-msr.h>
#include <processor.h>
#define VMCS_SIZE 0x1000
static uint32_t revision_id;

enum vmcs_fields {
	VMCS_VM_INSTRUCTION_ERROR = 0x4400,
};

static const char *vm_errs[] = {
	"Success",
	"VMCALL in vmx-root",
	"VMCLEAR with invalid address",
	"VMCLEAR with VMXON ptr",
	"VMLAUNCH with non-clear VMCS",
	"VMRESUME non-launched VMCS",
	"VMRESUME after VMXOFF",
	"VM entry with invalid control fields",
	"VM entry with invalid host-state fields",
	"VMPTRLD with invalid address",
	"VMPTRLD with VMXON ptr",
	"VMPTRLD with incorrect revision ID",
	"VM read/write from/to unsupported component",
	"VM write to read-only component",
	"[reserved]",
	"VMXON in vmx-root",
	"VM entry with invalid executive VMCS pointer",
	"VM entry with non-launched VMCS",
	"VM entry something?",
	"VMCALL with non-clear VMCS",
	"VMCALL with invalid VM exit control fields",
	"[reserved]",
	"VMCALL with incorrect revision ID",
	"VMXOFF in dual monitor",
	"VMCALL with invalid SMM features",
	"VM entry with invalid VM execution control fields",
	"VM entry with events blocked by mov ss",
	"[reserved]",
	"invalid operand to INVEPT/INVVPID"
};

static inline unsigned long vmcs_readl(unsigned long field)
{
	unsigned long val;
	asm volatile("vmread %%rdx, %%rax" : "=a"(val) : "d"(field) : "cc");
	return val;
}

static inline const char *vmcs_read_vminstr_err(void)
{
	uint32_t err = vmcs_readl(VMCS_VM_INSTRUCTION_ERROR);
	if(err >= array_len(vm_errs)) {
		panic("invalid error code from vm instruction error field");
	}
	return vm_errs[err];
}

static inline void vmcs_writel(unsigned long field, unsigned long val)
{
	uint8_t err;
	asm volatile("vmwrite %%rax, %%rdx" : "=q"(err) : "a"(val), "d"(field) : "cc");
	if(unlikely(err)) {
		panic("vmwrite error: %s", vmcs_read_vminstr_err());
	}
}

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


__attribute__((used)) static void x86_64_vmentry_failed(void)
{
	printk("HERE! %s\n", vmcs_read_vminstr_err());
	for(;;);
}

void x86_64_vmexit_handler(struct processor *proc)
{
	/* so, in theory we now have a valid pointer to a processor struct, and a stack
	 * to work in (see code below). */

}

void x86_64_vmenter(struct processor *proc)
{
	/* VMCS does not deal with CPU registers, so we must save and restore them. */

	asm volatile(
			"pushf;"
			"cmp $0, %0;"
			"mov %c[cr2](%%rcx), %%rax;"
			"mov %%rax, %%cr2;"
			"mov %c[rax](%%rcx), %%rax;"
			"mov %c[rbx](%%rcx), %%rbx;"
			"mov %c[rdx](%%rcx), %%rdx;"
			"mov %c[rdi](%%rcx), %%rdi;"
			"mov %c[rsi](%%rcx), %%rsi;"
			"mov %c[rbp](%%rcx), %%rbp;"
			"mov %c[r8](%%rcx),  %%r8;"
			"mov %c[r9](%%rcx),  %%r9;"
			"mov %c[r10](%%rcx), %%r10;"
			"mov %c[r11](%%rcx), %%r11;"
			"mov %c[r12](%%rcx), %%r12;"
			"mov %c[r13](%%rcx), %%r13;"
			"mov %c[r14](%%rcx), %%r14;"
			"mov %c[r15](%%rcx), %%r15;"
			"mov %c[rcx](%%rcx), %%rcx;" //this kills rcx
			"jne launched;"
			"vmlaunch; jmp failed;"
			"launched: vmresume; failed:"
			"popf;"
			"call x86_64_vmentry_failed;"
			/* okay, let's have an assembly stub for a VM exit */
			".global vmexit_point;"
			"vmexit_point:;"
			"mov %%rax, %c[rax](%%rsp);"
			"mov %%rbx, %c[rbx](%%rsp);"
			"mov %%rcx, %c[rcx](%%rsp);"
			"mov %%rdx, %c[rdx](%%rsp);"
			"mov %%rdi, %c[rdi](%%rsp);"
			"mov %%rsi, %c[rsi](%%rsp);"
			"mov %%rbp, %c[rbp](%%rsp);"
			"mov %%r8,  %c[r8](%%rsp);"
			"mov %%r9,  %c[r9](%%rsp);"
			"mov %%r10, %c[r10](%%rsp);"
			"mov %%r11, %c[r11](%%rsp);"
			"mov %%r12, %c[r12](%%rsp);"
			"mov %%r13, %c[r13](%%rsp);"
			"mov %%r14, %c[r14](%%rsp);"
			"mov %%r15, %c[r15](%%rsp);"
			"mov %c[procptr](%%rsp), %%rdi;"
			"mov %c[hrsp](%%rsp), %%rsp;"
			"jmp x86_64_vmexit_handler;"
			::
			"r"(proc->arch.launched),
			"c"(proc->arch.vcpu_state_regs),
			[rax]"i"(REG_RAX*8),
			[rbx]"i"(REG_RBX*8),
			[rcx]"i"(REG_RCX*8),
			[rdx]"i"(REG_RDX*8),
			[rdi]"i"(REG_RDI*8),
			[rsi]"i"(REG_RSI*8),
			[rbp]"i"(REG_RBP*8),
			[r8] "i"(REG_R8 *8),
			[r9] "i"(REG_R9 *8),
			[r10]"i"(REG_R10*8),
			[r11]"i"(REG_R11*8),
			[r12]"i"(REG_R12*8),
			[r13]"i"(REG_R13*8),
			[r14]"i"(REG_R14*8),
			[r15]"i"(REG_R15*8),
			[cr2]"i"(REG_CR2*8),
			[hrsp]"i"(HOST_RSP*8),
			[procptr]"i"(PROC_PTR));
}

void x86_64_start_vmx(struct processor *proc)
{
	x86_64_enable_vmx();

	proc->arch.launched = 0;
	memset(proc->arch.vcpu_state_regs, 0, sizeof(proc->arch.vcpu_state_regs));
	proc->arch.vcpu_state_regs[HOST_RSP] = (uintptr_t)proc->arch.kernel_stack;
	proc->arch.vcpu_state_regs[PROC_PTR] = (uintptr_t)proc;
	printk("Starting VMX system\n");
	proc->arch.vmcs = mm_physical_alloc(VMCS_SIZE, PM_TYPE_DRAM, true);
	uint32_t *vmcs_rev = (uint32_t *)(proc->arch.vmcs + PHYSICAL_MAP_START);
	*vmcs_rev = revision_id & 0x7FFFFFFF;

	uint8_t error;
	asm volatile("vmptrld (%%rax); setna %0" : "=g"(error) : "a"(&proc->arch.vmcs) : "cc");
	if(error) {
		panic("failed to load VMCS region: %s", vmcs_read_vminstr_err());
	}

	printk("Got here.\n");






	printk("Launching!\n");

	x86_64_vmenter(proc);

	for(;;);
	asm volatile("vmlaunch; setna %0" : "=g"(error) :: "cc");
	if(error) {
		panic("error occurred during VMLAUNCH: %s", vmcs_read_vminstr_err());
	}
	for(;;);
}

