#pragma once

#include <twz/alloc.h>
#include <twz/obj.h>

struct btree_val {
	const void *mv_data;
	size_t mv_size;
};

struct btree_node {
	struct btree_node *left, *right, *parent;
	struct btree_val mk, md;
	int color;
};

#define BTMAGIC 0xcafed00ddeadbeef

struct btree_hdr {
	struct twzoa_header oa;
	uint64_t magic;
	struct btree_node *root;
};

void bt_print_tree(struct object *obj, struct btree_hdr *);
struct btree_node *bt_lookup(struct object *obj, struct btree_hdr *, struct btree_val *k);
int bt_init(struct object *obj, struct btree_hdr *);
int bt_insert(struct object *obj,
  struct btree_hdr *,
  struct btree_val *k,
  struct btree_val *d,
  struct btree_node **);

struct btree_node *bt_next(struct object *obj, struct btree_hdr *hdr, struct btree_node *n);
struct btree_node *bt_prev(struct object *obj, struct btree_hdr *hdr, struct btree_node *n);
struct btree_node *bt_first(struct object *obj, struct btree_hdr *hdr);
struct btree_node *bt_last(struct object *obj, struct btree_hdr *hdr);

int bt_node_get(struct object *obj,
  struct btree_hdr *hdr,
  struct btree_node *n,
  struct btree_val *v);

int bt_node_getkey(struct object *obj,
  struct btree_hdr *hdr,
  struct btree_node *n,
  struct btree_val *v);

int bt_put(struct object *obj,
  struct btree_hdr *hdr,
  struct btree_val *k,
  struct btree_val *v,
  struct btree_node **node);
