#include <twzviewcall.h>
__asm__(
"__twz_viewcall:"
"push %rbp;"
"movq %rsp, %rbp;"
"pushq %r15;" //ridhi
"pushq %r14;" //ridlo
"pushq %r13;" //tidhi
"pushq %r12;" //tidlo
"pushq %rax;"
"movq $0x3ffec0001c00, %rax;"
"jmp *%rax;"
);

#include <stdio.h>
long twz_viewcall(long (*target)(), objid_t targetid, objid_t returnid, long a0, long a1, long a2, long a3, long a4, long a5)
{
	uint64_t tidlo = ID_LO(targetid);
	uint64_t tidhi = ID_HI(targetid);
	uint64_t ridlo = ID_LO(returnid);
	uint64_t ridhi = ID_HI(returnid);
	register long r8 __asm__("r8") = a4;
	register long r9 __asm__("r9") = a5;

	register long r12 __asm__("r12") = tidlo;
	register long r13 __asm__("r13") = tidhi;
	register long r14 __asm__("r14") = ridlo;
	register long r15 __asm__("r15") = ridhi;
	long ret;
	asm volatile(
			//"int $3;"
			"call __twz_viewcall;"
			//"int $3;"
			:"=a"(ret)
			:"a"((long)target), "r"(tidlo), "r"(tidhi), "r"(ridlo), "r"(ridhi),
			 "D"(a0), "S"(a1), "d"(a2), "c"(a3), "r"(r8), "r"(r9),
			 "r"(r12), "r"(r13), "r"(r14), "r"(r15)
			:"memory");
	return ret;
}

