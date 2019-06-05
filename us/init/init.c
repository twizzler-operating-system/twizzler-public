#include <stdlib.h>
#include <twz/debug.h>
#include <twz/name.h>
static void __name_boostrap(void);
int main(int argc, char **argv)
{
	debug_printf("%d: %p\n", argc, argv[0]);
	debug_printf("%d: %s\n", argc, argv[0]);
	debug_printf("Testing!:: %s\n", getenv("BSNAME"));

	__name_boostrap();
	objid_t id = 0;
	int r = twz_name_resolve(NULL, "test.text", NULL, 0, &id);
	debug_printf("NAME: " IDFMT " : %d\n", IDPR(id), r);
	for(;;)
		;
}
