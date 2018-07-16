#include <debug.h>
#include <stdio.h>

int main()
{
	debug_printf("shell - starting");
	printf("Hello, World!\n");
	char buf[128];
	for(;;) {
		if(fgets(buf, 127, stdin)) {
	//		printf("-> %s\n", buf);
			debug_printf("-> %s\n", buf);
		}

	}
	for(;;);
}

