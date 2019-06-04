#include <twz/_obj.h>
#include <twz/_objid.h>
#include <twz/_thrd.h>
#include <twz/debug.h>

static struct {
	void (*fn)(int, void *);
} _fault_table[NUM_FAULTS];

int __fault_obj_default(int fault, struct fault_object_info *info)
{
	debug_printf("A\n");
}

static __attribute__((used)) void __twz_fault_entry_c(int fault, void *_info)
{
	for(volatile long i = 0; i < 1000000000; i++)
		;
	if(fault == FAULT_OBJECT) {
		struct fault_object_info *fi = _info;
		debug_printf("FIO: %lx (%p)\n", fi->addr, &_fault_table[fault]);
		if(fi->addr == (uintptr_t)&_fault_table[fault] || !_fault_table[fault].fn) {
			if(__fault_obj_default(fault, _info) < 0) {
				twz_thread_exit();
			}
		}
	}
	if((fault >= NUM_FAULTS || !_fault_table[fault].fn) && fault != FAULT_OBJECT) {
		debug_printf("Unhandled exception: %d", fault);
		twz_thread_exit();
	}
	if(_fault_table[fault].fn) {
		_fault_table[fault].fn(fault, _info);
	}
}

/* TODO: arch-dep */

/* Stack comes in as mis-aligned (like any function call),
 * so maintain that alignment until the call below. */
asm(" \
                .extern __twz_fault_entry_c ;\
                __twz_fault_entry: ;\
                        pushq %rax;\
                        pushq %rbx;\
                        pushq %rcx;\
                        pushq %rdx;\
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
                        popq %rdx;\
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

void __twz_fault_entry(void);
void __twz_fault_init(void)
{
	uint64_t s, d;
	asm volatile("mov %%fs:0, %%rsi" : "=S"(s));
	asm volatile("mov %%fs:8, %%rdx" : "=d"(d));

	__seg_gs struct twzthread_repr *repr = (uintptr_t)OBJ_NULLPAGE_SIZE;

	/* have to do this manually, because fault handling during init
	 * may not use any global data (since the data object may not be mapped) */

	repr->faults[FAULT_OBJECT] = (struct faultinfo){
		.addr = (void *)__twz_fault_entry,
	};
}
