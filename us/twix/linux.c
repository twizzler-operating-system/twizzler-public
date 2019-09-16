#include "syscalls.h"
void __linux_init(void)
{
	__fd_sys_init();
}
