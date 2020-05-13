#include <time.h>

void abort();
char *ctime(const time_t *t)
{
	abort();
	return asctime(localtime(t));
}
