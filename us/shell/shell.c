#include <stdlib.h>
#include <twz/bstream.h>
#include <twz/debug.h>
#include <twz/name.h>
#include <twz/obj.h>
#include <twz/thread.h>

extern char **environ;
int main(int argc, char **argv)
{
	char buffer[1024];

	for(char **env = environ; *env != 0; env++) {
		char *thisEnv = *env;
		printf("%s\n", thisEnv);
	}

	char *username = getenv("USER");
	for(;;) {
		printf("%s@twz $ ", username);
		fflush(NULL);
		fgets(buffer, 1024, stdin);
		printf("got: %s\n", buffer);
	}
}
