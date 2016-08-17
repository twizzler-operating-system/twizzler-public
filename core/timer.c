#include <time.h>
#include <thread.h>
dur_nsec kernel_timer_tick(dur_nsec dt)
{
	/* TODO: temp hack */
	current_thread->flags |= THREAD_SCHEDULE;
	return 10000000; //1 ms
}

