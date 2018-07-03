#include <twzsec.h>
#include <twzsys.h>
#include <twzobj.h>

#include "blake2.h"

static void __sign(unsigned char *out, const unsigned char *in, size_t len)
{
	blake2b(out, 32, in, len, NULL, 0);
}

int twzsec_attach(objid_t sc, objid_t tid, int flags)
{
	return sys_attach(tid, sc, flags);
}

int twzsec_detach(objid_t sc, objid_t tid, int flags)
{
	return sys_detach(tid, sc, flags);
}

void twzsec_cap_create(struct cap *c, objid_t accessor, objid_t target, uint64_t flags)
{
	c->accessor = accessor;
	c->target = target;
	c->flags = flags;
	c->resv[0] = 0;
	c->resv[1] = 0;
	c->resv[2] = 0;
	__sign(c->sig, ((unsigned char *)c) + sizeof(c->sig), sizeof(*c) - sizeof(c->sig));
}

int twzsec_ctx_add(struct object *o, void *data, uint32_t type, uint64_t mask, uint32_t datalen)
{
	char *mem = __twz_ptr_lea(o, (void *)(o->mi->sz + OBJ_NULLPAGE_SIZE));
	if(mem == 0) {
		return -1;
	}
	size_t length = datalen + sizeof(struct dlgdata);
	if((o->mi->sz % 4096) + length > 4096) {
		mem += 4096 - (o->mi->sz % 4096);
	}
	struct dlgdata dd = {
		.mask = mask,
		.type = type,
		.length = datalen,
	};
	memcpy(mem, &dd, sizeof(dd));
	memcpy(mem + sizeof(dd), data, datalen);
	o->mi->sz += length;
	return 0;
}

