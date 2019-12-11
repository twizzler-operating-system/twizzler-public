#pragma once

#include <twz/alloc.h>
#include <twz/mutex.h>
#include <twz/obj.h>
#include <twz/persist.h>
#include <twz/tx.h>

struct btree_val {
	void *mv_data;
	size_t mv_size;
};

struct btree_node {
	struct btree_node *left, *right, *parent;
	struct btree_val mk, md;
	int color;
};

_Static_assert(sizeof(struct btree_node) <= __CL_SIZE, "");

#define BTMAGIC 0xcafed00ddeadbeef

#define __BT_HDR_LOG_SZ 512

struct btree_hdr {
	struct twzoa_header oa;
	char pad[64];
	uint64_t magic;
	struct btree_node *root;
	char pad1[64];
	struct mutex m;
	char pad2[64];
	struct twz_tx tx;
	char __tx_log[__BT_HDR_LOG_SZ];
};

void bt_print_tree(twzobj *obj, struct btree_hdr *);
struct btree_node *bt_lookup(twzobj *obj, struct btree_hdr *, struct btree_val *k);
int bt_init(twzobj *obj, struct btree_hdr *);
int bt_insert(twzobj *obj,
  struct btree_hdr *,
  struct btree_val *k,
  struct btree_val *d,
  struct btree_node **);

struct btree_node *bt_next(twzobj *obj, struct btree_hdr *hdr, struct btree_node *n);
struct btree_node *bt_prev(twzobj *obj, struct btree_hdr *hdr, struct btree_node *n);
struct btree_node *bt_first(twzobj *obj, struct btree_hdr *hdr);
struct btree_node *bt_last(twzobj *obj, struct btree_hdr *hdr);

int bt_node_get(twzobj *obj, struct btree_hdr *hdr, struct btree_node *n, struct btree_val *v);

int bt_node_getkey(twzobj *obj, struct btree_hdr *hdr, struct btree_node *n, struct btree_val *v);
struct btree_node *bt_delete(twzobj *obj, struct btree_hdr *hdr, struct btree_node *node);

int bt_put(twzobj *obj,
  struct btree_hdr *hdr,
  struct btree_val *k,
  struct btree_val *v,
  struct btree_node **node);

int bt_put_cmp(twzobj *obj,
  struct btree_hdr *hdr,
  struct btree_val *k,
  struct btree_val *v,
  struct btree_node **node,
  int (*cmp)(const struct btree_val *, const struct btree_val *));

struct btree_node *bt_lookup_cmp(twzobj *obj,
  struct btree_hdr *hdr,
  struct btree_val *k,
  int (*cmp)(const struct btree_val *, const struct btree_val *));

int bt_insert_cmp(twzobj *obj,
  struct btree_hdr *hdr,
  struct btree_val *k,
  struct btree_val *d,
  struct btree_node **nt,
  int (*cmp)(const struct btree_val *, const struct btree_val *));
