#include <syscall.h>

long syscall_invalidate_kso(struct syscall_kso_invl *invl, size_t count)
{
	return 0;
}

long syscall_attach(uint64_t palo, uint64_t pahi, uint64_t chlo, uint64_t chhi, uint64_t flags)
{
	return 0;
}

long syscall_detach(uint64_t palo, uint64_t pahi, uint64_t chlo, uint64_t chhi, uint64_t flags)
{
	return 0;
}

long syscall_ocreate(uint64_t olo, uint64_t ohi, uint64_t tlo, uint64_t thi, uint64_t flags)
{
	return 0;
}

long syscall_odelete(uint64_t olo, uint64_t ohi, uint64_t flags)
{
	return 0;
}

