#include <debug.h>
static __attribute__((used)) void __twz_fault_entry_c(void)
{
	debug_printf("DEBUG");
}

/* TODO: arch-dep */

/* Stack comes in as aligned (because this isn't really a call),
 * so maintain that alignment until the call below. */
asm (" \
		.extern __twz_fault_entry_c ;\
		__twz_fault_entry: ;\
			pushq %rax;\
			pushq %rbx;\
			pushq %rcx;\
			pushq %rbp;\
			pushq %r8;\
			pushq %r9;\
			pushq %r10;\
			pushq %r11;\
			pushq %r12;\
			pushq %r13;\
			pushq %r14;\
			pushq %r15;\
\
			call __twz_fault_entry_c ;\
\
			popq %r15;\
			popq %r14;\
			popq %r13;\
			popq %r12;\
			popq %r11;\
			popq %r10;\
			popq %r9;\
			popq %r8;\
			popq %rbp;\
			popq %rcx;\
			popq %rbx;\
			popq %rax;\
\
			popq %rdi;\
			popq %rsi;\
			popq %rsp;\
\
			subq $8, %rsp;\
			retq;\
	");
/* the subq is to move the stack back to where we stored the RIP for return.
 * We had to store the unmodified rsp from the kernel's frame to reload it
 * accurately, but the unmodified rsp is 8 above the location where we put the
 * return address */

#define TWZ_THREAD_REPR

#include <twzobj.h>
#include <twzslots.h>
#include <twzthread.h>

void __twz_fault_entry(void);

void __twz_fault_init(void)
{
	struct object thrd;
	twz_object_init(&thrd, TWZSLOT_THRD);
	struct twzthread_repr *repr = thrd.base;
	repr->thread_kso_data.faults[FAULT_OBJECT] = (struct faultinfo) {
		.addr = (void *)__twz_fault_entry,
	};
}

