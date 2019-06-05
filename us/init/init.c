#include <stdlib.h>
#include <twz/debug.h>
int main(int argc, char **argv)
{
	debug_printf("%d: %p\n", argc, argv[0]);
	debug_printf("%d: %s\n", argc, argv[0]);
	debug_printf("Testing!:: %s\n", getenv("BSNAME"));
	for(;;)
		;
}
