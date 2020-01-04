#include <syscall.h>

static long _tsc_ps = 0;

long arch_syscall_kconf(int cmd, long arg)
{
	switch(cmd) {
		case KCONF_ARCH_TSC_PSPERIOD:
			return _tsc_ps;
			break;
		default:
			return -EINVAL;
	}
}

void arch_syscall_kconf_set_tsc_period(long ps)
{
	_tsc_ps = ps;
}
