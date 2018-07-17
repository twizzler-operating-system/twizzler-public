#include <stdio.h>
#include <twzobj.h>
#include <twzkv.h>

void _cb(struct twzkv_item *key, struct twzkv_item *value)
{
	char buf[key->length+1];
	snprintf(buf, sizeof(buf), "%s", key->data);
	printf("%-30s -> " IDFMT "\n", buf, IDPR(*(objid_t *)value->data));
}

static struct object name_index;
static void __name_init(void)
{
	struct object kc;
	if(twz_object_open(&kc, 1, FE_READ) < 0) {
		return;
	}

	char *kcdata = twz_ptr_base(&kc);
	char *r = strstr(kcdata, "name=");
	if(!r) {
		return;
	}

	char *idstr = r + 5;
	objid_t id = 0;
	if(!twz_objid_parse(idstr, &id)) {
		return;
	}

	twz_object_open(&name_index, id, FE_READ | FE_WRITE);
}

int main()
{
	for(int i=0;i<20;i++)
	printf("Hello, World from a utility!\n");
	return 0;
	__name_init();
	twzkv_foreach(&name_index, _cb);
}

