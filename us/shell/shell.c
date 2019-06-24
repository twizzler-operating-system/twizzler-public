#include <stdlib.h>
#include <twz/bstream.h>
#include <twz/debug.h>
#include <twz/name.h>
#include <twz/obj.h>
#include <twz/thread.h>

int main(int argc, char **argv)
{
	char buffer[1024];
	for(;;) {
		printf("> ");
		fflush(NULL);
		fgets(buffer, 1024, stdin);
		printf("got: %s\n", buffer);
	}
}
