#pragma once

uint64_t x86_64_early_wait_ns(int64_t ns);
struct multiboot;
void x86_64_init(uint32_t magic, struct multiboot *mth);
struct processor;
void x86_64_cpu_secondary_entry(struct processor *proc);
void x86_64_lapic_init_percpu(void);
void arch_syscall_kconf_set_tsc_period(long ps);
void idt_init_secondary(void);
void idt_init(void);
void x86_64_vm_kernel_context_init(void);

#include <memory.h>

void x86_64_memory_record(uintptr_t addr,
  size_t len,
  enum memory_type type,
  enum memory_subtype st);
void x86_64_register_kernel_region(uintptr_t addr, size_t len);
void x86_64_register_initrd_region(uintptr_t addr, size_t len);
void x86_64_reclaim_initrd_region(void);
