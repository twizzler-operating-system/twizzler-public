
#include <debug.h>
#include <twzsys.h>

int main() {
	__sys_debug_print("Test!\n", 6);
	debug_printf("Hello, World!\n");
	for(;;);
}

void _start() {
	main();
}

