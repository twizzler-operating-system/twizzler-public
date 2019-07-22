#pragma once

#include <twz/_objid.h>
#include <twz/btree.h>

struct keyring_hdr {
	objid_t dfl_pub;
	objid_t dfl_pri;

	struct btree_hdr bt;
};

int twz_keyring_lookup(struct object *obj,
  unsigned char *fp,
  size_t fplen,
  objid_t *pub,
  objid_t *pri);

int twz_keyring_insert(struct object *obj,
  unsigned char *fp,
  size_t fplen,
  objid_t pub,
  objid_t pri);
