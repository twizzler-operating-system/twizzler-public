#pragma once

#include <system.h>

struct rbnode {
	uint64_t __parent;
	struct rbnode *left, *right;
};

struct rbroot {
	struct rbnode *node;
};

#define RBINIT                                                                                     \
	(struct rbroot)                                                                                \
	{                                                                                              \
		.node = NULL                                                                               \
	}

void rb_delete(struct rbnode *node, struct rbroot *root);
void rb_link_node(struct rbnode *node, struct rbnode *parent, struct rbnode **link);
struct rbnode *rb_first(const struct rbroot *root);
struct rbnode *rb_last(const struct rbroot *root);
struct rbnode *rb_next(const struct rbnode *node);
struct rbnode *rb_prev(const struct rbnode *node);
void __rb_insert(struct rbnode *node, struct rbroot *root);

#define rb_entry(ptr, type, member) container_of(ptr, type, member)

#define rb_insert(root, _new, type, memb, compar)                                                  \
	({                                                                                             \
		bool result = true;                                                                        \
		struct rbnode **link = &(root)->node, *_parent = NULL;                                     \
		while(*link) {                                                                             \
			_parent = *link;                                                                       \
			type *_node = rb_entry(_parent, type, memb);                                           \
			int _r = compar(_node, _new);                                                          \
			if(_r == 0) {                                                                          \
				result = false;                                                                    \
				break;                                                                             \
			} else if(_r > 0)                                                                      \
				link = &(*link)->left;                                                             \
			else                                                                                   \
				link = &(*link)->right;                                                            \
		}                                                                                          \
		if(result) {                                                                               \
			rb_link_node(&(_new)->node, _parent, link);                                            \
			__rb_insert(&(_new)->node, (root));                                                    \
		}                                                                                          \
		result;                                                                                    \
	})

#define rb_search(root, key, type, memb, compar_key)                                               \
	({                                                                                             \
		struct rbnode *_node = (root)->node;                                                       \
		struct rbnode *_res = NULL;                                                                \
		while(_node) {                                                                             \
			type *_stuff = rb_entry(_node, type, memb);                                            \
			int _r = compar_key(_stuff, key);                                                      \
			if(_r > 0)                                                                             \
				_node = _node->left;                                                               \
			else if(_r < 0)                                                                        \
				_node = _node->right;                                                              \
			else {                                                                                 \
				_res = _node;                                                                      \
				break;                                                                             \
			}                                                                                      \
		}                                                                                          \
		_res;                                                                                      \
	})
