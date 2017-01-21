#pragma once

struct __attribute__((packed)) x86_64_syscall_frame {
	uint64_t r10, r9, r8, rdx, rsi, rdi, rax, r11, rcx, rsp, cs;
};


struct arch_thread {
	struct x86_64_syscall_frame tcb;
};

