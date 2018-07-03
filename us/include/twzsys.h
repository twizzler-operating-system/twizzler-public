#pragma once
#include <stdint.h>
#include <stddef.h>
#include <twz.h>

#define SYS_NULL        0
#define SYS_THRD_SPAWN  1
#define SYS_DEBUG_PRINT 2
#define SYS_INVL_KSO    3
#define SYS_ATTACH      4
#define SYS_DETACH      5
#define SYS_BECOME      6
#define SYS_THRD_SYNC   7
#define SYS_OCREATE     8
#define SYS_ODELETE     9
#define SYS_THRD_CTL    10

static inline long __syscall6(long n, long a1, long a2,
	long a3, long a4, long a5, long a6)
{
	unsigned long ret;
	register long r8 __asm__("r8") = a4;
	register long r9 __asm__("r9") = a5;
	register long r10 __asm__("r10") = a6;
	__asm__ __volatile__ ("syscall; setb %%cl"
			: "=a"(ret)
			: "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r8), "r"(r9), "r"(r10)
			: "r11", "rcx", "memory");
	return ret;           
}

static inline long __sys_debug_print(const char *str, size_t len)
{
	return __syscall6(SYS_DEBUG_PRINT, (long)str, len, 0, 0, 0, 0);
}

static inline long sys_ocreate(objid_t id, objid_t srcid, int flags)
{
	return __syscall6(SYS_OCREATE, ID_LO(id), ID_HI(id),
			ID_LO(srcid), ID_HI(srcid), flags, 0);
}

static inline long sys_attach(objid_t pid, objid_t cid, int flags)
{
	return __syscall6(SYS_ATTACH, ID_LO(pid), ID_HI(pid),
			ID_LO(cid), ID_HI(cid), flags, 0);
}

static inline long sys_detach(objid_t pid, objid_t cid, int flags)
{
	return __syscall6(SYS_DETACH, ID_LO(pid), ID_HI(pid),
			ID_LO(cid), ID_HI(cid), flags, 0);
}

struct sys_invalidate_op {
	objid_t id;
	uint64_t offset;
	uint32_t length;
	uint16_t flags;
	uint16_t result;
} __packed;

#define KSO_INVL_RES_OK 0
#define KSO_INVL_RES_ERR -1
#define KSOI_VALID 1

static inline long sys_invalidate(struct sys_invalidate_op *invl, size_t count)
{
	return __syscall6(SYS_INVL_KSO, (long)invl, count, 0, 0, 0, 0);
}

#include <time.h>

#define THREAD_SYNC_SLEEP 0
#define THREAD_SYNC_WAKE  1

static inline long sys_thread_sync(int operation, int *addr, int arg, struct timespec *spec)
{
	return __syscall6(SYS_THRD_SYNC, operation, (long)addr, arg, (long)spec, 0, 0);
}

/* TODO: arch-dep */
struct sys_become_args {
	objid_t target_view;
	uint64_t target_rip;
	uint64_t rax;
	uint64_t rbx;
	uint64_t rcx;
	uint64_t rdx;
	uint64_t rdi;
	uint64_t rsi;
	uint64_t rsp;
	uint64_t rbp;
	uint64_t r8;
	uint64_t r9;
	uint64_t r10;
	uint64_t r11;
	uint64_t r12;
	uint64_t r13;
	uint64_t r14;
	uint64_t r15;
};

static inline long sys_thrd_spawn(objid_t tid, objid_t srcid,
		struct sys_become_args *ba, int flags)
{
	return __syscall6(SYS_THRD_SPAWN, ID_LO(tid), ID_HI(tid),
			ID_LO(srcid), ID_HI(srcid), (long)ba, flags);
}

static inline long sys_become(objid_t sid, struct sys_become_args *ba)
{
	return __syscall6(SYS_BECOME, ID_LO(sid), ID_HI(sid), (long)ba, 0, 0, 0);
}

#define THRD_CTL_SET_FS 1
#define THRD_CTL_SET_GS 2

static inline long sys_thrd_ctl(int op, long arg)
{
	return __syscall6(SYS_THRD_CTL, op, arg, 0, 0, 0, 0);
}

