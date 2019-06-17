#include <stdlib.h>
#include <twz/bstream.h>
#include <twz/debug.h>
#include <twz/name.h>
#include <twz/obj.h>
#include <twz/thread.h>

void tmain(void *a)
{
	(void)a;
	for(;;)
		;
}

struct object bs;
int main(int argc, char **argv)
{
	debug_printf("HELLO FROM TERM\n");

	for(;;)
		;
	struct thread t;
	int r;
	r = twz_thread_spawn(&t, &(struct thrd_spawn_args){ .start_func = tmain, .arg = &bs });

	struct thread *w = &t;
	uint64_t info;
	r = twz_thread_wait(1, &w, (int[]){ THRD_SYNC_READY }, NULL, &info);
	debug_printf("WAIT RET %d: %ld\n", r, info);

	for(;;)
		;
}
