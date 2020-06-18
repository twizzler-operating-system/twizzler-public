#pragma once

#include <twz/__twz.h>

#include <stddef.h>
#include <stdint.h>

#include <twz/_objid.h>
#include <twz/_sys.h>

__must_check static inline long
__syscall6(long n, long a1, long a2, long a3, long a4, long a5, long a6)
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

__attribute__((deprecated)) static inline long sys_vmap(const void *p, int cmd, long arg)
{
	return __syscall6(SYS_VMAP, (long)p, cmd, arg, 0, 0, 0);
}

__must_check static inline long sys_kconf(int cmd, long arg)
{
	return __syscall6(SYS_KCONF, cmd, arg, 0, 0, 0, 0);
}

__must_check static inline long sys_ocreate(int flags, objid_t kuid, objid_t src, objid_t *id)
{
	return __syscall6(
	  SYS_OCREATE, ID_LO(kuid), ID_HI(kuid), ID_LO(src), ID_HI(src), flags, (long)id);
}

__must_check static inline long sys_ocopy(objid_t dest,
  objid_t src,
  size_t doff,
  size_t soff,
  size_t len,
  int flags)
{
	return __syscall6(SYS_OCOPY, (long)&dest, (long)&src, doff, soff, len, flags);
}

__must_check static inline long sys_kqueue(objid_t id, enum kernel_queues kq, int flags)
{
	return __syscall6(SYS_KQUEUE, ID_LO(id), ID_HI(id), kq, flags, 0, 0);
}

__must_check static inline long sys_attach(objid_t pid, objid_t cid, int flags, int type)
{
	uint64_t sf = (uint64_t)(type & 0xffff) | (uint64_t)flags << 32;
	return __syscall6(SYS_ATTACH, ID_LO(pid), ID_HI(pid), ID_LO(cid), ID_HI(cid), sf, 0);
}

__must_check static inline long sys_detach(objid_t pid, objid_t cid, int flags, int type)
{
	int sysc = flags >> 16;
	int fl = flags & 0xffff;
	uint64_t sf =
	  (uint64_t)(type & 0xffff) | (uint64_t)fl << 32 | (uint64_t)((sysc & 0xffff) << 16);
	return __syscall6(SYS_DETACH, ID_LO(pid), ID_HI(pid), ID_LO(cid), ID_HI(cid), sf, 0);
}

__must_check static inline long sys_odelete(objid_t id, int flags)
{
	return __syscall6(SYS_ODELETE, ID_LO(id), ID_HI(id), flags, 0, 0, 0);
}

__must_check static inline long sys_invalidate(struct sys_invalidate_op *invl, size_t count)
{
	return __syscall6(SYS_INVL_KSO, (long)invl, count, 0, 0, 0, 0);
}

struct timespec;
__must_check static inline long sys_thread_sync(size_t count,
  struct sys_thread_sync_args *args,
  struct timespec *timeout)
{
	return __syscall6(SYS_THRD_SYNC, count, (long)args, (long)timeout, 0, 0, 0);
}

__must_check static inline long sys_thrd_spawn(objid_t tid,
  struct sys_thrd_spawn_args *tsa,
  int flags)
{
	return __syscall6(SYS_THRD_SPAWN, ID_LO(tid), ID_HI(tid), (long)tsa, flags, 0, 0);
}

__must_check static inline long sys_become(struct sys_become_args *ba)
{
	return __syscall6(SYS_BECOME, (long)ba, 0, 0, 0, 0, 0);
}

__must_check static inline long sys_thrd_ctl(int op, long arg)
{
	return __syscall6(SYS_THRD_CTL, op, arg, 0, 0, 0, 0);
}

__must_check static inline long sys_kaction(size_t count, struct sys_kaction_args *args)
{
	return __syscall6(SYS_KACTION, count, (long)args, 0, 0, 0, 0);
}

__must_check static inline long sys_opin(objid_t id, uint64_t *addr, int flags)
{
	return __syscall6(SYS_OPIN, ID_LO(id), ID_HI(id), (long)addr, flags, 0, 0);
}

__must_check static inline long sys_octl(objid_t id, int op, long arg1, long arg2, long arg3)
{
	return __syscall6(SYS_OCTL, ID_LO(id), ID_HI(id), op, arg1, arg2, arg3);
}

__must_check static inline long sys_otie(objid_t parent, objid_t child, int flags)
{
	return __syscall6(SYS_OTIE, ID_LO(parent), ID_HI(parent), ID_LO(child), ID_HI(child), flags, 0);
}
