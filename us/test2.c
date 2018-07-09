
#include <debug.h>
#include <twzsys.h>

#include <twzthread.h>

_Atomic int id=0;

void _tt(void *a __unused)
{
	debug_printf("Hello world from spawned thread!\n");

	int x = ++id;
	for(;;) {
		for(volatile int i = 0;i<400000000 / x;i++);
		debug_printf("%d: a", x);
		//if(x == 1)
		//	for(volatile int i = 0;i<40000000;i++);
	}
}

int main() {
	__sys_debug_print("Test!\n", 6);
	debug_printf("Hello, World!\n");
	struct twzthread th;
	struct twzthread th2;
	twz_thread_spawn(&th, _tt, NULL, NULL, 0);
	twz_thread_spawn(&th2, _tt, NULL, NULL, 0);
	for(;;) {
		//for(volatile int i = 0;i<400000000;i++);
		debug_printf("A");
	}
}


