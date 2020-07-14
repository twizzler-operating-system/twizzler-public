/* this red-black tree code has been modified from the Linux kernel rbtree implementation to fit our
 * needs. This file retains the GPL-2 License */

/*
 *   (C) 1999  Andrea Arcangeli <andrea@suse.de>
 *   (C) 2002  David Woodhouse <dwmw2@infradead.org>
 *   (C) 2012  Michel Lespinasse <walken@google.com>
 *   (C) 2020  Daniel Bittman <danielbittman1@gmail.com>

 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <lib/rb.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define RB_BLACK 1
#define RB_RED 0

static inline void rb_set_black(struct rbnode *n)
{
	n->__parent |= RB_BLACK;
}

static inline struct rbnode *rb_parent(const struct rbnode *n)
{
	return (void *)(n->__parent & ~RB_BLACK);
}

static void rb_set_parent_color(struct rbnode *n, struct rbnode *p, int color)
{
	n->__parent = (uint64_t)p | color;
}

static inline int rb_color(struct rbnode *n)
{
	return n->__parent & 1;
}

static void rb_change_child(struct rbnode *old,
  struct rbnode *new,
  struct rbnode *parent,
  struct rbroot *root)
{
	/* change the parent's child from old to new. If parent is NULL, then we're changing the root */
	if(parent) {
		if(parent->left == old) {
			parent->left = new;
		} else {
			parent->right = new;
		}
	} else {
		root->node = new;
	}
}

static void rb_rotate_set_parents(struct rbnode *old,
  struct rbnode *new,
  struct rbroot *root,
  int color)
{
	struct rbnode *parent = rb_parent(old);
	new->__parent = old->__parent;
	rb_set_parent_color(old, new, color);
	rb_change_child(old, new, parent, root);
}

void __rb_insert(struct rbnode *node, struct rbroot *root)
{
	struct rbnode *parent = rb_parent(node);

	while(true) {
		/* keep the invariant that the node is red */
		if(!parent) {
			/* we inserted to the root. Color the root black, and return. */
			rb_set_parent_color(node, NULL, RB_BLACK);
			break;
		}

		/* if we have a black parent, exit loop. Otherwise, there are two red nodes in a row, so
		 * correct that. */
		if(rb_color(parent) == RB_BLACK) {
			break;
		}

		struct rbnode *grand_parent = rb_parent(parent);
		struct rbnode *tmp = grand_parent->right;
		if(parent != tmp) {
			/* parent is grandparent's left; uncle is grandparent's right (tmp) */
			if(tmp && rb_color(tmp) == RB_RED) {
				/* node's uncle (and parent) is red. Flip the colors. But since grandparent's parent
				 * might be red, we'd have grandparent and great-grandparent red, which is not
				 * allowed. Recurse at grandparent. */
				rb_set_parent_color(tmp, grand_parent, RB_BLACK);
				rb_set_parent_color(parent, grand_parent, RB_BLACK);
				node = grand_parent;
				parent = rb_parent(node);
				rb_set_parent_color(node, parent, RB_RED);
				continue;
			}
			/* since NULL is black, if we're here, node's uncle is black */
			tmp = parent->right;
			if(node == tmp) {
				/* we are the parent's right child. Left rotate @ parent. However, grandparent's
				 * children will not both be black, which is a violation. We'll fix that in a bit.*/
				tmp = node->left;
				parent->right = tmp;
				node->left = parent;
				if(tmp) {
					/* node had a left subtree. Set the root of that tree to black */
					rb_set_parent_color(tmp, parent, RB_BLACK);
				}
				/* parent's parent is now node. And we're red still. */
				rb_set_parent_color(parent, node, RB_RED);
				parent = node;
				tmp = node->right;
				/* there's an implicit swap between parent and node, but since we don't need node
				 * below, we don't set it. */
			}
			/* node's uncle is black && node is the parent's left child. Right rotate @ grand parent
			 */
			grand_parent->left = tmp;
			parent->right = grand_parent;
			if(tmp) {
				/* parent had a right subtree */
				rb_set_parent_color(tmp, grand_parent, RB_BLACK);
			}
			rb_rotate_set_parents(grand_parent, parent, root, RB_RED);
			break;
		} else {
			/* parent is grandparent's right child. Do the same as above, but mirror image. */
			tmp = grand_parent->left;
			if(tmp && rb_color(tmp) == RB_RED) {
				rb_set_parent_color(tmp, grand_parent, RB_BLACK);
				rb_set_parent_color(parent, grand_parent, RB_BLACK);
				node = grand_parent;
				parent = rb_parent(node);
				rb_set_parent_color(node, parent, RB_RED);
				continue;
			}

			tmp = parent->left;
			if(node == tmp) {
				tmp = node->right;
				parent->left = tmp;
				node->right = parent;
				if(tmp) {
					rb_set_parent_color(tmp, parent, RB_BLACK);
				}

				rb_set_parent_color(parent, node, RB_RED);
				parent = node;
				tmp = node->left;
			}

			grand_parent->right = tmp;
			parent->left = grand_parent;
			if(tmp) {
				rb_set_parent_color(tmp, grand_parent, RB_BLACK);
			}
			rb_rotate_set_parents(grand_parent, parent, root, RB_RED);
			break;
		}
	}
}

static struct rbnode *__rb_delete(struct rbnode *node, struct rbroot *root)
{
	struct rbnode *child = node->right;
	struct rbnode *tmp = node->left;
	struct rbnode *parent, *rebal;

	if(!tmp) {
		/* there's no more than one child. This is the easy case, because we can just point the
		 * parent to our child. */
		parent = rb_parent(node);
		rb_change_child(node, child, parent, root);
		if(child) {
			/* one child; update the child's parent. */
			child->__parent = node->__parent;
			rebal = NULL;
		} else {
			rebal = rb_color(node) == RB_BLACK ? parent : NULL;
		}
		tmp = parent;
	} else if(!child) {
		tmp->__parent = node->__parent;
		parent = rb_parent(node);
		rb_change_child(node, tmp, parent, root);
		rebal = NULL;
		tmp = parent;
	} else {
		/* okay, two children. */
		struct rbnode *succ = child, *child2;
		tmp = child->left;
		if(!tmp) {
			/* node's succ is its right child */
			parent = succ;
			child2 = succ->right;
		} else {
			/* node succ is leftmode under right subtree */
			do {
				parent = succ;
				succ = tmp;
				tmp = tmp->left;
			} while(tmp);
			child2 = succ->right;
			parent->left = child2;
			succ->right = child;
			rb_set_parent_color(child, succ, rb_color(child));
		}

		tmp = node->left;
		succ->left = tmp;
		rb_set_parent_color(tmp, succ, rb_color(tmp));
		tmp = rb_parent(node);
		rb_change_child(node, succ, tmp, root);

		if(child2) {
			succ->__parent = node->__parent;
			rb_set_parent_color(child2, parent, RB_BLACK);
			rebal = NULL;
		} else {
			uint64_t backup = succ->__parent;
			succ->__parent = node->__parent;
			rebal = (backup & RB_BLACK) ? parent : NULL;
		}
		tmp = succ;
	}
	return rebal;
}

static void __rb_del_rebal(struct rbnode *parent, struct rbroot *root)
{
	struct rbnode *node = NULL, *sibling, *tmp1, *tmp2;

	while(true) {
		/* invariant:
		 *  - node is black
		 *  - node is not the root
		 *  - all leaf paths going through parent and node have a black count that is 1 lower than
		 *  other paths */
		sibling = parent->right;
		if(node != sibling) {
			/* node is parent's left */
			if(rb_color(sibling) == RB_RED) {
				/* left rotate @ parent */
				tmp1 = sibling->left;
				parent->right = tmp1;
				sibling->left = parent;
				rb_set_parent_color(tmp1, parent, RB_BLACK);
				rb_rotate_set_parents(parent, sibling, root, RB_RED);
				sibling = tmp1;
			}
			tmp1 = sibling->right;
			if(!tmp1 || rb_color(tmp1) == RB_BLACK) {
				tmp2 = sibling->left;
				if(!tmp2 || rb_color(tmp2) == RB_BLACK) {
					rb_set_parent_color(sibling, parent, RB_RED);
					if(rb_color(parent) == RB_RED) {
						rb_set_black(parent);
					} else {
						node = parent;
						parent = rb_parent(node);
						if(parent) {
							continue;
						}
					}
					break;
				}

				tmp1 = tmp2->right;
				sibling->left = tmp1;
				tmp2->right = sibling;
				parent->right = tmp2;
				if(tmp1) {
					rb_set_parent_color(tmp1, sibling, RB_BLACK);
				}
				tmp1 = sibling;
				sibling = tmp2;
			}

			tmp2 = sibling->left;
			parent->right = tmp2;
			sibling->left = parent;
			rb_set_parent_color(tmp1, sibling, RB_BLACK);
			if(tmp2) {
				rb_set_parent_color(tmp2, parent, rb_color(tmp2));
			}
			rb_rotate_set_parents(parent, sibling, root, RB_BLACK);
			break;
		} else {
			sibling = parent->left;
			if(rb_color(sibling) == RB_RED) {
				tmp1 = sibling->right;
				parent->left = tmp1;
				sibling->right = parent;
				rb_set_parent_color(tmp1, parent, RB_BLACK);
				rb_rotate_set_parents(parent, sibling, root, RB_RED);
				sibling = tmp1;
			}
			tmp1 = sibling->left;
			if(!tmp1 || rb_color(tmp1) == RB_BLACK) {
				tmp2 = sibling->right;
				if(!tmp2 || rb_color(tmp2) == RB_BLACK) {
					/* Case 2 - sibling color flip */
					rb_set_parent_color(sibling, parent, RB_RED);
					if(rb_color(parent) == RB_RED) {
						rb_set_black(parent);
					} else {
						node = parent;
						parent = rb_parent(node);
						if(parent) {
							continue;
						}
					}
					break;
				}
				/* Case 3 - left rotate at sibling */
				tmp1 = tmp2->left;
				sibling->right = tmp1;
				tmp2->left = sibling;
				parent->left = tmp2;
				if(tmp1) {
					rb_set_parent_color(tmp1, sibling, RB_BLACK);
				}
				tmp1 = sibling;
				sibling = tmp2;
			}
			/* Case 4 - right rotate at parent + color flips */
			tmp2 = sibling->right;
			parent->left = tmp2;
			sibling->right = parent;
			rb_set_parent_color(tmp1, sibling, RB_BLACK);
			if(tmp2) {
				rb_set_parent_color(tmp2, parent, rb_color(tmp2));
			}
			rb_rotate_set_parents(parent, sibling, root, RB_BLACK);
			break;
		}
	}
}

void rb_delete(struct rbnode *node, struct rbroot *root)
{
	struct rbnode *re = __rb_delete(node, root);
	if(re) {
		__rb_del_rebal(re, root);
	}
}

void rb_link_node(struct rbnode *node, struct rbnode *parent, struct rbnode **link)
{
	node->left = node->right = NULL;
	rb_set_parent_color(node, parent, RB_RED);
	*link = node;
}

struct rbnode *rb_first(const struct rbroot *root)
{
	struct rbnode *n = root->node;
	if(!n)
		return NULL;
	while(n->left)
		n = n->left;
	return n;
}

struct rbnode *rb_last(const struct rbroot *root)
{
	struct rbnode *n = root->node;
	if(!n)
		return NULL;
	while(n->right)
		n = n->right;
	return n;
}

struct rbnode *rb_next(const struct rbnode *node)
{
	struct rbnode *parent;

	if(node->right) {
		node = node->right;
		while(node->left)
			node = node->left;
		return (struct rbnode *)node;
	}

	while((parent = rb_parent(node)) && node == parent->right)
		node = parent;
	return parent;
}

struct rbnode *rb_prev(const struct rbnode *node)
{
	struct rbnode *parent;

	if(node->left) {
		node = node->left;
		while(node->right)
			node = node->right;
		return (struct rbnode *)node;
	}

	while((parent = rb_parent(node)) && node == parent->left)
		node = parent;
	return parent;
}

#if 0
void rb_print(struct rbnode *node, int indent)
{
	if(!node) {
		printf("%*s*\n", indent, "");
		return;
	}
	struct stuff *s = rb_entry(node, struct stuff, node);
	printf("%*s%d\n", indent, "", s->key);
	rb_print(node->left, indent + 2);
	rb_print(node->right, indent + 2);
}
#endif
