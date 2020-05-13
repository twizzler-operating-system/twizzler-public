#include <stdio.h>
#include <twz/gate.h>
#include <unistd.h>

#include <twz/debug.h>

void gate_fn()
{
	debug_printf("HELLO\n");
	// write(1, "gate_fn called\n", 15);
}

void bad_gate_fn()
{
	debug_printf("SHOULD NOT GET HERE\n");
}

TWZ_GATE(gate_fn, 0);
// TWZ_GATE(bad_gate_fn, 1);

int main()
{
	printf("Hello\n");
}
