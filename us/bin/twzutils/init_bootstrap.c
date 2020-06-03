#include <stdlib.h>
#include <unistd.h>

#include <twz/debug.h>

extern int __name_bootstrap(void);
int main()
{
	debug_printf("Bootstrapping naming system\n");
	if(__name_bootstrap() == -1) {
		debug_printf("Failed to bootstrap namer\n");
		abort();
	}
	unsetenv("BSNAME");
	setenv("TERM", "linux", 1);
	setenv("PATH", "/bin:/usr/bin", 1);

	execlp("/usr/bin/init", "init", NULL);
	return -1;
}
