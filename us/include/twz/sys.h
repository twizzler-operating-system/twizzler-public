#pragma once
#include <stddef.h>
#include <stdint.h>

#include <twz/_objid.h>
#include <twz/_sys.h>

static inline long __syscall6(long n, long a1, long a2, long a3, long a4, long a5, long a6)
{
	unsigned long ret;
	register long r8 __asm__("r8") = a4;
	register long r9 __asm__("r9") = a5;
	register long r10 __asm__("r10") = a6;
	__asm__ __volatile__("syscall;"
	                     : "=a"(ret)
	                     : "a"(n), "D"(a1), "S"(a2), "d"(a3), "r"(r8), "r"(r9), "r"(r10)
	                     : "r11", "rcx", "memory");
	return ret;
}

static inline long __sys_debug_print(const char *str, size_t len)
{
	return __syscall6(SYS_DEBUG_PRINT, (long)str, len, 0, 0, 0, 0);
}

static inline long sys_ocreate(int flags, objid_t kuid, objid_t src, objid_t *id)
{
	return __syscall6(
	  SYS_OCREATE, ID_LO(kuid), ID_HI(kuid), ID_LO(src), ID_HI(src), flags, (long)id);
}

static inline long sys_attach(objid_t pid, objid_t cid, int flags)
{
	return __syscall6(SYS_ATTACH, ID_LO(pid), ID_HI(pid), ID_LO(cid), ID_HI(cid), flags, 0);
}

static inline long sys_detach(objid_t pid, objid_t cid, int flags)
{
	return __syscall6(SYS_DETACH, ID_LO(pid), ID_HI(pid), ID_LO(cid), ID_HI(cid), flags, 0);
}

static inline long sys_invalidate(struct sys_invalidate_op *invl, size_t count)
{
	return __syscall6(SYS_INVL_KSO, (long)invl, count, 0, 0, 0, 0);
}

#include <time.h>

static inline long sys_thread_sync(size_t count,
  int *operation,
  long **addr,
  long *arg,
  long *res,
  struct timespec **spec)
{
	return __syscall6(
	  SYS_THRD_SYNC, count, (long)operation, (long)addr, (long)arg, (long)res, (long)spec);
}

static inline long sys_thrd_spawn(objid_t tid, struct sys_thrd_spawn_args *tsa, int flags)
{
	return __syscall6(SYS_THRD_SPAWN, ID_LO(tid), ID_HI(tid), (long)tsa, flags, 0, 0);
}

static inline long sys_become(objid_t sid, struct sys_become_args *ba)
{
	return __syscall6(SYS_BECOME, ID_LO(sid), ID_HI(sid), (long)ba, 0, 0, 0);
}

static inline long sys_thrd_ctl(int op, long arg)
{
	return __syscall6(SYS_THRD_CTL, op, arg, 0, 0, 0, 0);
}
