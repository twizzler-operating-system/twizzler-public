#include <string.h>
#include <twz/_err.h>
#include <twz/alloc.h>
#include <twz/btree.h>
#include <twz/debug.h>
#include <twz/obj.h>
enum {
	RED,
	BLACK,
};

__attribute__((const)) static inline struct btree_node *__c(void *x)
{
	return (struct btree_node *)(twz_ptr_local(x));
}

__attribute__((const)) static inline struct btree_node *__l(struct object *obj, void *x)
{
	if(x == NULL)
		return NULL;
	struct btree_node *r = twz_ptr_lea(obj, x);
	return r;
}

static int __cf_default(const struct btree_val *a, const struct btree_val *b)
{
	const unsigned char *ac = a->mv_data;
	const unsigned char *bc = b->mv_data;
	for(size_t i = 0;; i++, ac++, bc++) {
		//	debug_printf("((( %ld %ld %ld\n", i, a->mv_size, b->mv_size);
		if(i >= a->mv_size && i >= b->mv_size)
			return 0;
		if(i >= a->mv_size)
			return -1;
		if(i >= b->mv_size)
			return 1;
		int c = *ac - *bc;
		//	debug_printf("  ((( %d %x %x\n", c, *ac, *bc);
		if(c)
			return c;
	}
}

static void swap(int *x, int *y)
{
	int t = *x;
	*x = *y;
	*y = t;
}

/* A utility function to insert a new node with given key
   in BST */
/* lea: 3 */
/* can: 4 */
static struct btree_node *BSTInsert(struct object *obj,
  struct btree_node *root,
  struct btree_node *pt,
  struct btree_node **found,
  struct btree_val *key)
{
	/* If the tree is empty, return a new node */
	if(root == NULL) {
		return pt;
	}

	struct btree_val rkey = {
		.mv_data = twz_ptr_lea(obj, root->mk.mv_data),
		.mv_size = root->mk.mv_size,
	};

	/*struct btree_val pkey = {
	    .mv_data = twz_ptr_lea(obj, pt->mk.mv_data),
	    .mv_size = pt->mk.mv_size,
	};*/

	/* Otherwise, recur down the tree */
	// debug_printf("::::::: %p %p\n", key->mv_data, pkey.mv_data);
	int c = __cf_default(&rkey, key);
	// printf("COMPARE %ld %ld %lx %lx -> %d\n", rkey.mv_size, pkey.mv_size,
	// *(uint64_t*)rkey.mv_data, *(uint64_t*)pkey.mv_data, c);
	if(c > 0) {
		root->left = __c(BSTInsert(obj, __l(obj, root->left), pt, found, key));
		__l(obj, root->left)->parent = __c(root);
	} else if(c < 0) {
		root->right = __c(BSTInsert(obj, __l(obj, root->right), pt, found, key));
		__l(obj, root->right)->parent = __c(root);
	} else {
		root->md.mv_data = pt->md.mv_data;
		root->md.mv_size = pt->md.mv_size;
		*found = root;
	}

	/* return the (unchanged) node pointer */
	return root;
}

/* lea: 11 */
/* can: 8 */
/* eq: 2 */
static void rotateLeft(struct object *obj, struct btree_node **root, struct btree_node **pt)
{
	struct btree_node *pt_right = __l(obj, __l(obj, (*pt))->right);

	__l(obj, (*pt))->right = pt_right->left;

	if(__l(obj, (*pt))->right != NULL)
		__l(obj, __l(obj, (*pt))->right)->parent = *pt;

	pt_right->parent = __l(obj, (*pt))->parent;

	if(__l(obj, (*pt))->parent == NULL)
		*root = __c(pt_right);

	else if(*pt == __l(obj, __l(obj, (*pt))->parent)->left)
		__l(obj, __l(obj, (*pt))->parent)->left = __c(pt_right);

	else
		__l(obj, __l(obj, (*pt))->parent)->right = __c(pt_right);

	pt_right->left = *pt;
	__l(obj, (*pt))->parent = __c(pt_right);
}

/* lea: 11 */
/* can: 8 */
/* eq: 2 */
static void rotateRight(struct object *obj, struct btree_node **root, struct btree_node **pt)
{
	struct btree_node *pt_left = __l(obj, __l(obj, (*pt))->left);

	__l(obj, (*pt))->left = pt_left->right;

	if(__l(obj, (*pt))->left != NULL)
		__l(obj, __l(obj, (*pt))->left)->parent = *pt;

	pt_left->parent = __l(obj, (*pt))->parent;

	if(__l(obj, (*pt))->parent == NULL)
		*root = __c(pt_left);

	else if(*pt == __l(obj, __l(obj, (*pt))->parent)->left)
		__l(obj, __l(obj, (*pt))->parent)->left = __c(pt_left);

	else
		__l(obj, __l(obj, (*pt))->parent)->right = __c(pt_left);

	pt_left->right = *pt;
	__l(obj, (*pt))->parent = __c(pt_left);
}

// This function fixes violations caused by BST insertion
/* lea: 7 */
/* can: 8 */
/* eq: 4 */
static void fixViolation(struct object *obj, struct btree_node **root, struct btree_node **pt)
{
	struct btree_node *parent_pt = NULL;
	struct btree_node *grand_parent_pt = NULL;

	while((*pt != *root) && (__l(obj, (*pt))->color != BLACK)
	      && (__l(obj, __l(obj, (*pt))->parent)->color == RED)) {
		parent_pt = __l(obj, __l(obj, (*pt))->parent);
		grand_parent_pt = __l(obj, parent_pt->parent);

		/*  Case : A
		    Parent of pt is left child of Grand-parent of pt */
		if(parent_pt == __l(obj, grand_parent_pt->left)) {
			struct btree_node *uncle_pt = __l(obj, grand_parent_pt->right);

			/* Case : 1
			   The uncle of pt is also red
			   Only Recoloring required */
			if(uncle_pt != NULL && uncle_pt->color == RED) {
				grand_parent_pt->color = RED;
				parent_pt->color = BLACK;
				uncle_pt->color = BLACK;
				*pt = __c(grand_parent_pt);

			} else {
				/* Case : 2
				   pt is right child of its parent
				   Left-rotation required */
				if(*pt == parent_pt->right) {
					struct btree_node *tmp = __c(parent_pt);
					rotateLeft(obj, root, &tmp);
					*pt = tmp;
					parent_pt = __l(obj, __l(obj, (*pt))->parent);
				}

				/* Case : 3
				   pt is left child of its parent
				   Right-rotation required */
				struct btree_node *tmp = __c(grand_parent_pt);
				rotateRight(obj, root, &tmp);
				grand_parent_pt = __l(obj, tmp);
				swap(&parent_pt->color, &grand_parent_pt->color);
				*pt = __c(parent_pt);
			}
			/* Case : B
			   Parent of pt is right child of Grand-parent of pt */
		} else {
			struct btree_node *uncle_pt = __l(obj, grand_parent_pt->left);

			/*  Case : 1
			    The uncle of pt is also red
			    Only Recoloring required */
			if((uncle_pt != NULL) && (uncle_pt->color == RED)) {
				grand_parent_pt->color = RED;
				parent_pt->color = BLACK;
				uncle_pt->color = BLACK;
				*pt = __c(grand_parent_pt);
			} else {
				/* Case : 2
				   pt is left child of its parent
				   Right-rotation required */
				if(*pt == parent_pt->left) {
					struct btree_node *tmp = __c(parent_pt);
					rotateRight(obj, root, &tmp);
					*pt = tmp;
					parent_pt = __l(obj, __l(obj, (*pt))->parent);
				}

				/* Case : 3
				   pt is right child of its parent
				   Left-rotation required */
				struct btree_node *tmp = __c(grand_parent_pt);
				rotateLeft(obj, root, &tmp);
				grand_parent_pt = __l(obj, tmp);
				swap(&parent_pt->color, &grand_parent_pt->color);
				*pt = __c(parent_pt);
			}
		}
	}

	__l(obj, (*root))->color = BLACK;
}

// Function to insert a new node with given data
/* lea: 0 */
/* can: 1 */
/* eq: 0 */
int bt_insert(struct object *obj,
  struct btree_hdr *hdr,
  struct btree_val *k,
  struct btree_val *d,
  struct btree_node **nt)
{
	if(hdr->magic != BTMAGIC) {
		return -EINVAL;
	}
	struct btree_node *pt = oa_hdr_alloc(obj, &hdr->oa, sizeof(struct btree_node));

	pt = __l(obj, pt);
	pt->left = pt->right = pt->parent = NULL;
	pt->color = 0;
	pt->mk.mv_size = k->mv_size;
	pt->md.mv_size = d->mv_size;
	pt->mk.mv_data = k->mv_data;
	k->mv_data = twz_ptr_lea(obj, k->mv_data);

	pt->md.mv_data = d->mv_data;

	// Do a normal BST insert
	struct btree_node *e = NULL;
	hdr->root = __c(BSTInsert(obj, __l(obj, hdr->root), pt, &e, k));
	if(e) {
		if(nt)
			*nt = e;
		oa_hdr_free(obj, &hdr->oa, __c(pt));
		return 1;
	}
	if(nt)
		*nt = pt;
	pt = __c(pt);

	// fix Red Black Tree violations
	fixViolation(obj, &hdr->root, &pt);
	return 0;
}

/* lea: 1 */
/* can: 0 */
/* eq: 0 */
static struct btree_node *_dolookup(struct object *obj,
  struct btree_node *root,
  struct btree_val *k)
{
	// debug_printf(":: root=%p\n", root);
	if(!root)
		return NULL;
	struct btree_val rootkey = {
		.mv_data = twz_ptr_lea(obj, root->mk.mv_data),
		.mv_size = root->mk.mv_size,
	};
	int c = __cf_default(&rootkey, k);
	// debug_printf("CMP: %s %s: %d\n", (uint32_t*)rootkey.mv_data, (uint32_t*)k->mv_data, c);
	if(!c) {
		return root;
	} else if(c > 0) {
		return _dolookup(obj, __l(obj, root->left), k);
	} else {
		return _dolookup(obj, __l(obj, root->right), k);
	}
}
static struct btree_node *__bt_leftmost(struct object *obj, struct btree_node *n)
{
	while(n->left) {
		n = __l(obj, n->left);
	}
	return n;
}

static struct btree_node *__bt_rightmost(struct object *obj, struct btree_node *n)
{
	while(n->right) {
		n = __l(obj, n->right);
	}
	return n;
}

struct btree_node *bt_last(struct object *obj, struct btree_hdr *hdr)
{
	if(hdr->magic != BTMAGIC) {
		return NULL;
	}
	struct btree_node *n = __l(obj, hdr->root);
	return __bt_rightmost(obj, n);
}

struct btree_node *bt_first(struct object *obj, struct btree_hdr *hdr)
{
	if(hdr->magic != BTMAGIC) {
		return NULL;
	}
	struct btree_node *n = __l(obj, hdr->root);
	return __bt_leftmost(obj, n);
}

struct btree_node *bt_prev(struct object *obj, struct btree_hdr *hdr, struct btree_node *n)
{
	if(hdr->magic != BTMAGIC) {
		return NULL;
	}
	if(!n)
		return n;
	if(n->left)
		return __bt_rightmost(obj, __l(obj, n->left));
	struct btree_node *p = __l(obj, n->parent);
	while(p && n == __l(obj, p->left)) {
		n = p;
		p = __l(obj, p->parent);
	}
	return p;
}

struct btree_node *bt_next(struct object *obj, struct btree_hdr *hdr, struct btree_node *n)
{
	if(hdr->magic != BTMAGIC) {
		return NULL;
	}
	if(!n)
		return n;
	if(n->right)
		return __bt_leftmost(obj, __l(obj, n->right));
	struct btree_node *p = __l(obj, n->parent);
	while(p && n == __l(obj, p->right)) {
		n = p;
		p = __l(obj, p->parent);
	}
	return p;
}

int bt_init(struct object *obj, struct btree_hdr *hdr)
{
	hdr->magic = BTMAGIC;
	return oa_hdr_init(obj, &hdr->oa, 0x2000, OBJ_MAXSIZE - 0x8000);
}

struct btree_node *bt_lookup(struct object *obj, struct btree_hdr *hdr, struct btree_val *k)
{
	if(hdr->magic != BTMAGIC) {
		return NULL;
	}
	return _dolookup(obj, __l(obj, hdr->root), k);
}

int bt_node_get(struct object *obj,
  struct btree_hdr *hdr,
  struct btree_node *n,
  struct btree_val *v)
{
	(void)hdr;
	v->mv_size = n->md.mv_size;
	v->mv_data = twz_ptr_lea(obj, n->md.mv_data);
	return 0;
}

int bt_node_getkey(struct object *obj,
  struct btree_hdr *hdr,
  struct btree_node *n,
  struct btree_val *v)
{
	(void)hdr;
	v->mv_size = n->mk.mv_size;
	v->mv_data = twz_ptr_lea(obj, n->mk.mv_data);
	return 0;
}

int bt_put(struct object *obj,
  struct btree_hdr *hdr,
  struct btree_val *k,
  struct btree_val *v,
  struct btree_node **node)
{
	void *dest_k = oa_hdr_alloc(obj, &hdr->oa, k->mv_size);
	if(!dest_k)
		return -ENOSPC;
	void *vdest_k = twz_ptr_lea(obj, dest_k);
	memcpy(vdest_k, k->mv_data, k->mv_size);

	void *dest_v = oa_hdr_alloc(obj, &hdr->oa, v->mv_size);
	if(!dest_v) {
		oa_hdr_free(obj, &hdr->oa, dest_k);
		return -ENOSPC;
	}
	void *vdest_v = twz_ptr_lea(obj, dest_v);
	memcpy(vdest_v, v->mv_data, v->mv_size);

	struct btree_val nk = { .mv_data = dest_k, .mv_size = k->mv_size };
	struct btree_val nv = { .mv_data = dest_v, .mv_size = v->mv_size };

	return bt_insert(obj, hdr, &nk, &nv, node);
}

#include <twz/debug.h>
static void _doprint_tree(struct object *obj, int indent, struct btree_node *root)
{
	debug_printf("%*s %p", indent, "", root);
	if(!root) {
		debug_printf("\n");
		return;
	}
	struct btree_val rootkey = {
		.mv_data = twz_ptr_lea(obj, root->mk.mv_data),
		.mv_size = root->mk.mv_size,
	};
	debug_printf(" [");
	for(unsigned int i = 0; i < 16 && rootkey.mv_size < 30 && i < rootkey.mv_size; i++) {
		debug_printf("%x", ((unsigned char *)rootkey.mv_data)[i]);
	}
	debug_printf("]\n");
	if(!root->left && !root->right)
		return;

	_doprint_tree(obj, indent + 2, __l(obj, root->right));
	_doprint_tree(obj, indent + 2, __l(obj, root->left));
}

void bt_print_tree(struct object *obj, struct btree_hdr *hdr)
{
	_doprint_tree(obj, 0, __l(obj, hdr->root));
}
