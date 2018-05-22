#pragma once

#include <stdint.h>

static inline long _syscall0(long num)
{
	long ret;
	asm volatile("syscall" : "=a"(ret) : "a"(num) : "rcx", "r11", "memory");
	return ret;
}

static inline long _syscall1(long num, long a1)
{
	long ret;
	asm volatile("syscall" : "=a"(ret) : "a"(num), "D"(a1) : "rcx", "r11", "memory");
	return ret;
}

static inline long _syscall2(long num, long a1, long a2)
{
	long ret;
	asm volatile("syscall" : "=a"(ret) : "a"(num), "D"(a1), "S"(a2) : "rcx", "r11", "memory");
	return ret;
}

static inline long _syscall3(long num, long a1, long a2, long a3)
{
	long ret;
	asm volatile("syscall" : "=a"(ret) : "a"(num), "D"(a1), "S"(a2), "d"(a3) : "rcx", "r11", "memory");
	return ret;
}

static inline long _syscall4(long num, long a1, long a2, long a3, long a4)
{
	register long r8 asm("r8") = a4;
	long ret;
	asm volatile("syscall" : "=a"(ret) : "a"(num), "D"(a1), "S"(a2), "d"(a3), "r"(r8) : "rcx", "r11", "memory");
	return ret;
}

static inline long _syscall5(long num, long a1, long a2, long a3, long a4, long a5)
{
	register long r8 asm("r8") = a4;
	register long r9 asm("r9") = a5;
	long ret;
	asm volatile("syscall" : "=a"(ret) : "a"(num), "D"(a1), "S"(a2), "d"(a3), "r"(r8), "r"(r9) : "rcx", "r11", "memory");
	return ret;
}

static inline long _syscall6(long num, long a1, long a2, long a3, long a4, long a5, long a6)
{
	register long r8 asm("r8") = a4;
	register long r9 asm("r9") = a5;
	register long r10 asm("r10") = a6;
	long ret;
	asm volatile("syscall" : "=a"(ret) : "a"(num), "D"(a1), "S"(a2), "d"(a3), "r"(r8), "r"(r9), "r"(r10) : "rcx", "r11", "memory");
	return ret;
}

static inline long _syscallg10(long num, __int128 g1)
{
	long ret;
	asm volatile("syscall" : "=a"(ret) : "a"(num), "D"((uint64_t)g1), "S"((uint64_t)(g1 >> 64)) : "rcx", "r11", "memory");
	return ret;
}

static inline long _syscallg11(long num, __int128 g1, long a1)
{
	long ret;
	asm volatile("syscall" : "=a"(ret) : "a"(num), "D"((uint64_t)g1), "S"((uint64_t)(g1 >> 64)), "d"(a1) : "rcx", "r11", "memory");
	return ret;
}

static inline long _syscallg12(long num, __int128 g1, long a1, long a2)
{
	register long r8 asm("r8") = a2;
	long ret;
	asm volatile("syscall" : "=a"(ret) : "a"(num), "D"((uint64_t)g1), "S"((uint64_t)(g1 >> 64)), "d"(a1), "r"(r8) : "rcx", "r11", "memory");
	return ret;
}

static inline long _syscallg13(long num, __int128 g1, long a1, long a2, long a3)
{
	register long r8 asm("r8") = a2;
	register long r9 asm("r9") = a3;
	long ret;
	asm volatile("syscall" : "=a"(ret) : "a"(num), "D"((uint64_t)g1), "S"((uint64_t)(g1 >> 64)), "d"(a1), "r"(r8), "r"(r9) : "rcx", "r11", "memory");
	return ret;
}

static inline long _syscallg14(long num, __int128 g1, long a1, long a2, long a3, long a4)
{
	register long r8 asm("r8") = a2;
	register long r9 asm("r9") = a3;
	register long r10 asm("r10") = a4;
	long ret;
	asm volatile("syscall" : "=a"(ret) : "a"(num), "D"((uint64_t)g1), "S"((uint64_t)(g1 >> 64)), "d"(a1), "r"(r8), "r"(r9), "r"(r10) : "rcx", "r11", "memory");
	return ret;
}

static inline __int128 _syscallrg(long num)
{
	uint64_t hi, lo;
	asm volatile("syscall" : "=a"(lo), "=d"(hi) : "a"(num));
	__int128 ret = hi;
	ret <<= 64;
	ret |= lo;
	return ret;
}

