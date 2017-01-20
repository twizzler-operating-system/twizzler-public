#pragma once
#define X86_MSR_APIC_BASE 0x1B
  #define X86_MSR_APIC_BASE_BSP    0x100
  #define X86_MSR_APIC_BASE_X2MODE 0x400
  #define X86_MSR_APIC_BASE_ENABLE 0x800

#define X86_MSR_FS_BASE             0xc0000100


static inline void x86_64_rdmsr(uint32_t msr, uint32_t *lo, uint32_t *hi)
{
	asm volatile("rdmsr" : "=a"(*lo), "=d"(*hi) : "c"(msr));
}
 
static inline void x86_64_wrmsr(uint32_t msr, uint32_t lo, uint32_t hi)
{
	asm volatile("wrmsr" : : "a"(lo), "d"(hi), "c"(msr));
}

