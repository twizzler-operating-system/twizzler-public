#include <rand.h>
#include <syscall.h>

_Static_assert(sizeof(long) == 8, "");

static uint64_t reset_code = 0;
static struct spinlock _lock;

long syscall_kconf(int cmd, long arg)
{
	int ret = 0;
	switch(cmd) {
		case KCONF_RDRESET:
			if(atomic_load(&reset_code) == 0) {
				spinlock_acquire_save(&_lock);
				if(atomic_load(&reset_code) == 0) {
					rand_getbytes(&reset_code, sizeof(reset_code), 0);
					atomic_thread_fence(memory_order_seq_cst);
					assert(reset_code != 0);
				}
				spinlock_release_restore(&_lock);
			}
			return reset_code;
			break;
		default:
			ret = arch_syscall_kconf(cmd, arg);
	}
	return ret;
}
