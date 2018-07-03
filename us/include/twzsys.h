#pragma once
#include <stdint.h>
#include <fbsdsys.h>
#include <stddef.h>
#include <twz.h>
#define SYS_umtx                454
#define	SYS_twistie_setview     551
#define	SYS_twistie_ocreate     552
#define	SYS_twistie_omutate	    553
#define	SYS_twistie_invlview	554
#define	SYS_twistie_thrd_spawn	555
#define SYS_twistie_attach      556
#define SYS_twistie_detach      557
#define SYS_twistie_become      558

static inline long __syscall6(long n, long a1, long a2,
	long a3, long a4, long a5, long a6)
{
	unsigned long ret;
	char err = 0;
	register long r10 __asm__("r10") = a4;
	register long r8 __asm__("r8") = a5;
	register long r9 __asm__("r9") = a6;
	__asm__ __volatile__ ("syscall; setb %%cl" : "=a"(ret), "=c"(err) : "a"(n), "D"(a1), "S"(a2),
			"d"(a3), "r"(r10), "r"(r8), "r"(r9) : "r11", "memory");
	if(err) {
		return -ret;
	}
	return ret;           
}

#define USYNC_PROCESS_SHARED    0x0001
#define UMTX_OP_WAIT            2
#define UMTX_OP_WAKE            3
struct stat;
struct sigaction;
struct thr_param;
struct iovec;
static inline int fbsd_sys_umtx(void *obj, int op, unsigned long val)
{
	return __syscall6(SYS_umtx, (long)obj, op, val, 0, 0, 0);
}

static inline int fbsd_sys_twistie_omutate(uint64_t lo, uint64_t hi, int flags)
{
	return __syscall6(SYS_twistie_omutate, lo, hi, flags, 0, 0, 0);
}

static inline int fbsd_sys_twistie_ocreate(uint64_t idlo, uint64_t idhi, uint64_t slo, uint64_t shi, int flags)
{
	return __syscall6(SYS_twistie_ocreate, idlo, idhi, slo, shi, flags, 0);
}
struct thr_param;
static inline int fbsd_sys_thr_new(struct thr_param *param, int psize)
{
	return __syscall6(SYS_thr_new, (long)param, psize, 0, 0, 0, 0);
}

static inline int fbsd_sys_thr_exit(void *state)
{
	return __syscall6(SYS_thr_exit, (long)state, 0, 0, 0, 0, 0);
}

static inline int fbsd_thr_exit(void *state)
{
	return __syscall6(SYS_thr_exit, (long)state, 0, 0, 0, 0, 0);
}

static inline int fbsd_twistie_thrd_spawn(uint64_t idlo, uint64_t idhi, struct thr_param *param, int ps)
{
	return __syscall6(SYS_twistie_thrd_spawn, idlo, idhi, (long)param, ps, 0, 0);
}

static inline int fbsd_twistie_attach(uint64_t secidlo, uint64_t secidhi, uint64_t tidlo, uint64_t tidhi, int flags)
{
	return __syscall6(SYS_twistie_attach, secidlo, secidhi, tidlo, tidhi, flags, 0);
}

static inline int fbsd_twistie_detach(uint64_t secidlo, uint64_t secidhi, uint64_t tidlo, uint64_t tidhi, uint64_t jmp, int flags)
{
	return __syscall6(SYS_twistie_detach, secidlo, secidhi, tidlo, tidhi, jmp, flags);
}

static inline int fbsd_access(const char *path, int mode)
{
	return __syscall6(SYS_access, (long)path, mode, 0, 0, 0, 0);
}

static inline int fbsd_chroot(const char *path)
{
	return __syscall6(SYS_chroot, (long)path, 0, 0, 0, 0, 0);
}

static inline int fbsd_chdir(const char *path)
{
	return __syscall6(SYS_chdir, (long)path, 0, 0, 0, 0, 0);
}

static inline ssize_t fbsd_readv(int fd, struct iovec *v, int c)
{
	return __syscall6(SYS_readv, fd, (long)v, c, 0, 0, 0);
}

static inline ssize_t fbsd_writev(int fd, const struct iovec *v, int c)
{
	return __syscall6(SYS_writev, fd, (long)v, c, 0, 0, 0);
}

static inline ssize_t fbsd_read(int fd, void *buf, size_t len)
{
	return __syscall6(SYS_read, fd, (long)buf, len, 0, 0, 0);
}

static inline ssize_t fbsd_write(int fd, const void *buf, size_t len)
{
	return __syscall6(SYS_write, fd, (long)buf, len, 0, 0, 0);
}

static inline int fbsd_sigaction(int sig, const struct sigaction * restrict act,
		struct sigaction * restrict oact)
{
	return __syscall6(SYS_sigaction, sig, (long)act, (long)oact, 0, 0, 0);
}

static inline void fbsd__exit(int code)
{
	__syscall6(SYS_exit, code, 0, 0, 0, 0, 0);
}

static inline void fbsd_close(int fd)
{
	__syscall6(SYS_close, fd, 0, 0, 0, 0, 0);
}

static inline int fbsd_sysarch(int cmd, void *p)
{
	return __syscall6(SYS_sysarch, cmd, (long)p, 0, 0, 0, 0);
}

static inline int fbsd_symlink(const char *n1, const char *n2)
{
	return __syscall6(SYS_symlink, (long)n1, (long)n2, 0, 0, 0, 0);
}

static inline int fbsd_readlink(const char *restrict path, char *restrict buf, size_t bs)
{
	return __syscall6(SYS_readlink, (long)path, (long)buf, bs, 0, 0, 0);
}

static inline int fbsd_getpid(void)
{
	return __syscall6(SYS_getpid, 0, 0, 0, 0, 0, 0);
}

static inline int fbsd_open(const char *path, int flags, long mode)
{
	return __syscall6(SYS_open, (long)path, flags, mode, 0, 0, 0);
}

static inline int fbsd_stat(const char * restrict path, struct stat * restrict sb)
{
	return __syscall6(SYS_stat, (long)path, (long)sb, 0, 0, 0, 0);
}

static inline int fbsd_thr_self(long *tid)
{
	return __syscall6(SYS_thr_self, (long)tid, 0, 0, 0, 0, 0);
}

static inline int fbsd_fstat(int fd, struct stat * restrict sb)
{
	return __syscall6(SYS_fstat, fd, (long)sb, 0, 0, 0, 0);
}

static inline int fbsd_twistie_invlview(uint64_t idlo, uint64_t idhi, int start, int end, int flags)
{
	return __syscall6(SYS_twistie_invlview, idlo, idhi, start, end, flags, 0);
}

static inline int fbsd_twistie_setview(uint64_t lo, uint64_t hi, int flags)
{
	return __syscall6(SYS_twistie_setview, lo, hi, flags, 0, 0, 0);
}

static inline int fbsd_twistie_become(uint64_t slo, uint64_t shi, uint64_t vlo, uint64_t vhi, uint64_t *jmp, int flags)
{
	return __syscall6(SYS_twistie_become, slo, shi, vlo, vhi, (long)jmp, flags);
}

