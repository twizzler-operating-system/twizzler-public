#include <twzthread.h>
#include <bstream.h>
#include <twzobj.h>

#include <debug.h>

static void _input_thrd(void *arg)
{
	debug_printf("term input - starting");
	for(;;) {

	}
}

int main()
{
	debug_printf("term - starting");
	struct twzthread it;
	if(twz_thread_spawn(&it, _input_thrd, NULL, NULL, 0) < 0) {
		debug_printf("Failed to spawn input thread");
		return 1;
	}

	twz_thread_ready();
	for(;;);
}

