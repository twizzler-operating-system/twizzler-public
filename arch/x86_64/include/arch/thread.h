#pragma once

struct __attribute__((packed)) x86_64_exception_frame
{
	uint64_t r15, r14, r13, r12, rbp, rbx, r11, r10, r9, r8, rax, rcx, rdx, rsi, rdi;
	uint64_t int_no, err_code;
	uint64_t rip, cs, rflags, userrsp, ss;
};

struct __attribute__((packed)) x86_64_syscall_frame {
	uint64_t r10, r9, r8, rdx, rsi, rdi, rax, r11, rcx, rsp, cs;
};


struct arch_thread {
	struct x86_64_syscall_frame syscall;
	struct x86_64_exception_frame exception;
	bool was_syscall;
};

