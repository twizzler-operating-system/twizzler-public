#pragma once

#include <twz/__twz.h>

#include <stddef.h>
#include <stdint.h>
#include <twz/_objid.h>
#include <twz/_types.h>

enum name_ent_type {
	NAME_ENT_REGULAR,
	NAME_ENT_NAMESPACE,
	NAME_ENT_SYMLINK,
};

#define NAME_ENT_VALID 1

#define NAMESPACE_MAGIC 0xa13a1300bbbbcccc

struct twz_name_ent {
	objid_t id;
	uint32_t flags;
	uint16_t resv0;
	uint8_t type;
	uint8_t resv1;
	size_t dlen;
	char name[];
} __attribute__((packed));

struct twz_namespace_hdr {
	uint64_t magic;
	uint32_t version;
	uint32_t flags;
	struct twz_name_ent ents[];
};

__must_check int twz_hier_resolve_name(twzobj *ns,
  const char *path,
  int flags,
  struct twz_name_ent *ent);

#define TWZ_HIER_SYM 1

__must_check int twz_hier_assign_name(twzobj *ns, const char *name, int type, objid_t id);
int twz_hier_namespace_new(twzobj *ns, twzobj *parent, const char *name, uint64_t);
ssize_t twz_hier_get_entry(twzobj *ns, size_t pos, struct twz_name_ent **ent);
int twz_hier_readlink(twzobj *ns, const char *path, char *buf, size_t bufsz);
int twz_hier_remove_name(twzobj *ns, const char *name);
