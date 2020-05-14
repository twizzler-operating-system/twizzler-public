#include <time.h>

void abort();
char *ctime(const time_t *t)
{
	return asctime(localtime(t));
}
