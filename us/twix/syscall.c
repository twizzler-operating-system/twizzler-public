
#include <stdint.h>

struct twix_register_frame {
	uint64_t r15;
	uint64_t r14;
	uint64_t r13;
	uint64_t r12;
	uint64_t r11;
	uint64_t r10;
	uint64_t r9;
	uint64_t r8;
	uint64_t rbp;
	uint64_t rsi;
	uint64_t rdi;
	uint64_t rdx;
	uint64_t rbx;
	uint64_t rsp;
};

#include <debug.h>

long __twix_syscall_target_c(long num, struct twix_register_frame *frame)
{
	debug_printf("TWIX entry: %ld, %p %lx\n", num, frame, frame->rsp);
	return 0;
}

asm (
	".global __twix_syscall_target;"
	"__twix_syscall_target:;"
	"movq %rsp, %rcx;"
	"andq $-16, %rsp;"
	"pushq %rcx;"

	"pushq %rbx;"
	"pushq %rdx;"
	"pushq %rdi;"
	"pushq %rsi;"
	"pushq %rbp;"
	"pushq %r8;"
	"pushq %r9;"
	"pushq %r10;"
	"pushq %r11;"
	"pushq %r12;"
	"pushq %r13;"
	"pushq %r14;"
	"pushq %r15;"

	"movq %rsp, %rsi;"
	"movq %rax, %rdi;"
	"call __twix_syscall_target_c;"

	"popq %r15;"
	"popq %r14;"
	"popq %r13;"
	"popq %r12;"
	"popq %r11;"
	"popq %r10;"
	"popq %r9;"
	"popq %r8;"
	"popq %rbp;"
	"popq %rsi;"
	"popq %rdi;"
	"popq %rdx;"
	"popq %rbx;"
	"popq %rsp;"
	"ret;"
);


