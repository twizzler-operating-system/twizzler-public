#include <features.h>

#define START "_start"

#include "crt_arch.h"

#include "../../../../../../us/include/twz/debug.h"
int main();
void _init() __attribute__((weak));
void _fini() __attribute__((weak));
_Noreturn int __libc_start_main(int (*)(), int, char **, void (*)(), void (*)(), void (*)());

void __twz_fault_init(void);

void _start_c(long *p)
{
	int argc = p[0];
	char **argv = (void *)(p + 1);
	__twz_fault_init();
	__libc_start_main(main, argc, argv, _init, _fini, 0);
}
