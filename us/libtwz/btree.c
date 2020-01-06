#include <string.h>
#include <twz/_err.h>
#include <twz/alloc.h>
#include <twz/btree.h>
#include <twz/debug.h>
#include <twz/obj.h>

/* TODO: persist */

enum {
	RED,
	BLACK,
};

#if 1
#define TXOPT_RECORD_TMP(...)
#define TXOPT_RECORD(...)
#define TX_RECORD_COMMIT(...)
#define TXOPT_START(...)
#define TXOPT_END
#define TXOPT_COMMIT
#define TXCHECK(...)

#define _clwb(...)
#define _pfence()
//#define mutex_acquire(...)
//#define mutex_release(...)
#endif

static void _doprint_tree(twzobj *obj, int indent, struct btree_node *root);
__attribute__((const)) static inline struct btree_node *__c(void *x)
{
	return (struct btree_node *)(twz_ptr_local(x));
}

__attribute__((const)) static inline struct btree_node *__l(twzobj *obj, void *x)
{
	if(x == NULL)
		return NULL;
	// return (void *)((uintptr_t)x + obj->base);
	struct btree_node *r = twz_object_lea(obj, x);
	return r;
}

static int __cf_default(const struct btree_val *a, const struct btree_val *b)
{
	const unsigned char *ac = a->mv_data;
	const unsigned char *bc = b->mv_data;
	for(size_t i = 0;; i++, ac++, bc++) {
		if(i >= a->mv_size && i >= b->mv_size)
			return 0;
		if(i >= a->mv_size)
			return -1;
		if(i >= b->mv_size)
			return 1;
		int c = *ac - *bc;
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
static struct btree_node *BSTInsert(twzobj *obj,
  struct btree_node *root,
  struct btree_node *pt,
  struct btree_node **found,
  struct btree_val *key,
  int (*cmp)(const struct btree_val *, const struct btree_val *))
{
	/* If the tree is empty, return a new node */
	if(root == NULL) {
		return pt;
	}

	struct btree_val rkey = {
		.mv_data = twz_object_lea(obj, root->kp),
		.mv_size = root->ks,
	};

	/* Otherwise, recur down the tree */
	int c = -cmp(key, &rkey);
	if(c > 0) {
		struct btree_node *nn = __c(BSTInsert(obj, __l(obj, root->left), pt, found, key, cmp));
		if(root->left != nn) {
			struct btree_node *vnn = __l(obj, nn);
			vnn->parent = __c(root);
			_clwb_len(vnn, sizeof(*vnn));
			_pfence();
			root->left = nn;
			_clwb_len(root, sizeof(*root));
			_pfence();
			//__l(obj, root->left)->parent = __c(root);
		}
	} else if(c < 0) {
		struct btree_node *nn = __c(BSTInsert(obj, __l(obj, root->right), pt, found, key, cmp));
		if(root->right != nn) {
			struct btree_node *vnn = __l(obj, nn);
			vnn->parent = __c(root);
			_clwb_len(vnn, sizeof(*vnn));
			_pfence();
			root->right = nn;
			_clwb_len(root, sizeof(*root));
			_pfence();
			//__l(obj, root->right)->parent = __c(root);
		}
	} else {
		/* TODO: not totally consistent */
		root->dp = pt->dp;
		root->ds = pt->ds;
		_clwb_len(root, sizeof(*root));
		_pfence();
		*found = root;
	}

	/* return the (unchanged) node pointer */
	return root;
}

/* lea: 11 */
/* can: 8 */
/* eq: 2 */
static void rotateLeft(twzobj *obj,
  struct twz_tx *tx,
  struct btree_node **root,
  struct btree_node **pt)
{
	struct btree_node *pt_right = __l(obj, __l(obj, (*pt))->right);

	int rcode;
	TXOPT_START(obj, tx, rcode)
	{
		struct btree_node *v_pt = __l(obj, (*pt));

		TXOPT_RECORD_TMP(tx, &v_pt->right);
		TXOPT_RECORD_TMP(tx, &v_pt->parent);
		TXOPT_RECORD_TMP(tx, &pt_right->parent);
		TXOPT_RECORD_TMP(tx, &pt_right->left);
		TX_RECORD_COMMIT(tx);

		v_pt->right = pt_right->left;

		if(v_pt->right != NULL) {
			struct btree_node *v = __l(obj, v_pt->right);
			TXOPT_RECORD(tx, &v->parent);
			v->parent = *pt;
		}

		pt_right->parent = v_pt->parent;

		if(v_pt->parent == NULL) {
			TXOPT_RECORD(tx, &*root);
			*root = __c(pt_right);
		}

		else if(*pt == __l(obj, v_pt->parent)->left) {
			struct btree_node *v = __l(obj, v_pt->parent);
			TXOPT_RECORD(tx, &v->left);
			v->left = __c(pt_right);
		} else {
			struct btree_node *v = __l(obj, v_pt->parent);
			TXOPT_RECORD(tx, &v->right);
			v->right = __c(pt_right);
		}

		pt_right->left = *pt;
		v_pt->parent = __c(pt_right);
		TXOPT_COMMIT;
	}
	TXOPT_END;
}

/* lea: 11 */
/* can: 8 */
/* eq: 2 */
static void rotateRight(twzobj *obj,
  struct twz_tx *tx,
  struct btree_node **root,
  struct btree_node **pt)
{
	struct btree_node *pt_left = __l(obj, __l(obj, (*pt))->left);

	int rcode;
	TXOPT_START(obj, tx, rcode)
	{
		struct btree_node *v_pt = __l(obj, (*pt));

		TXOPT_RECORD_TMP(tx, &v_pt->left);
		TXOPT_RECORD_TMP(tx, &v_pt->parent);
		TXOPT_RECORD_TMP(tx, &pt_left->parent);
		TXOPT_RECORD_TMP(tx, &pt_left->right);
		TX_RECORD_COMMIT(tx);

		v_pt->left = pt_left->right;

		if(v_pt->left != NULL) {
			struct btree_node *v = __l(obj, v_pt->left);
			TXOPT_RECORD(tx, &v->parent);
			v->parent = *pt;
		}

		pt_left->parent = v_pt->parent;

		if(v_pt->parent == NULL) {
			TXOPT_RECORD(tx, &*root);
			*root = __c(pt_left);
		}

		else if(*pt == __l(obj, v_pt->parent)->left) {
			struct btree_node *v = __l(obj, v_pt->parent);
			TXOPT_RECORD(tx, &v->left);
			v->left = __c(pt_left);
		} else {
			struct btree_node *v = __l(obj, v_pt->parent);
			TXOPT_RECORD(tx, &v->right);
			v->right = __c(pt_left);
		}

		pt_left->right = *pt;
		v_pt->parent = __c(pt_left);

		TXOPT_COMMIT;
	}
	TXOPT_END;
}

// This function fixes violations caused by BST insertion
/* lea: 7 */
/* can: 8 */
/* eq: 4 */
static void fixViolation(twzobj *obj,
  struct twz_tx *tx,
  struct btree_node **root,
  struct btree_node **pt)
{
	struct btree_node *parent_pt = NULL;
	struct btree_node *grand_parent_pt = NULL;

	int rcode;
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
				TXOPT_START(obj, tx, rcode);
				{
					TXOPT_RECORD_TMP(tx, &grand_parent_pt->color);
					TXOPT_RECORD_TMP(tx, &parent_pt->color);
					TXOPT_RECORD_TMP(tx, &uncle_pt->color);
					TX_RECORD_COMMIT(tx);
					grand_parent_pt->color = RED;
					parent_pt->color = BLACK;
					uncle_pt->color = BLACK;
					TXOPT_COMMIT;
				}
				TXOPT_END;
				*pt = __c(grand_parent_pt);

			} else {
				/* Case : 2
				   pt is right child of its parent
				   Left-rotation required */
				if(*pt == parent_pt->right) {
					struct btree_node *tmp = __c(parent_pt);
					rotateLeft(obj, tx, root, &tmp);
					*pt = tmp;
					parent_pt = __l(obj, __l(obj, (*pt))->parent);
				}

				/* Case : 3
				   pt is left child of its parent
				   Right-rotation required */
				struct btree_node *tmp = __c(grand_parent_pt);
				rotateRight(obj, tx, root, &tmp);
				grand_parent_pt = __l(obj, tmp);

				TXOPT_START(obj, tx, rcode)
				{
					TXOPT_RECORD_TMP(tx, &parent_pt->color);
					TXOPT_RECORD_TMP(tx, &grand_parent_pt->color);
					TX_RECORD_COMMIT(tx);
					swap(&parent_pt->color, &grand_parent_pt->color);
					TXOPT_COMMIT;
				}
				TXOPT_END;

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
				int rcode;
				TXOPT_START(obj, tx, rcode)
				{
					TXOPT_RECORD_TMP(tx, &grand_parent_pt->color);
					TXOPT_RECORD_TMP(tx, &parent_pt->color);
					TXOPT_RECORD_TMP(tx, &uncle_pt->color);
					TX_RECORD_COMMIT(tx);
					grand_parent_pt->color = RED;
					parent_pt->color = BLACK;
					uncle_pt->color = BLACK;
					TXOPT_COMMIT;
				}
				TXOPT_END;
				*pt = __c(grand_parent_pt);
			} else {
				/* Case : 2
				   pt is left child of its parent
				   Right-rotation required */
				if(*pt == parent_pt->left) {
					struct btree_node *tmp = __c(parent_pt);
					rotateRight(obj, tx, root, &tmp);
					*pt = tmp;
					parent_pt = __l(obj, __l(obj, (*pt))->parent);
				}

				/* Case : 3
				   pt is right child of its parent
				   Left-rotation required */
				struct btree_node *tmp = __c(grand_parent_pt);
				rotateLeft(obj, tx, root, &tmp);
				grand_parent_pt = __l(obj, tmp);

				TXOPT_START(obj, tx, rcode)
				{
					TXOPT_RECORD_TMP(tx, &parent_pt->color);
					TXOPT_RECORD_TMP(tx, &grand_parent_pt->color);
					TX_RECORD_COMMIT(tx);
					swap(&parent_pt->color, &grand_parent_pt->color);
					TXOPT_COMMIT;
				}
				TXOPT_END;
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
int bt_insert_cmp(twzobj *obj,
  struct btree_hdr *hdr,
  struct btree_val *k,
  struct btree_val *d,
  struct btree_node **nt,
  int (*cmp)(const struct btree_val *, const struct btree_val *))
{
	if(hdr->magic != BTMAGIC) {
		return -EINVAL;
	}
	struct btree_node *pt = oa_hdr_alloc(obj, &hdr->oa, sizeof(struct btree_node));

	pt = __l(obj, pt);
	pt->left = pt->right = pt->parent = NULL;
	pt->color = 0;
	pt->ks = k->mv_size;
	pt->ds = d->mv_size;
	if(k->mv_size > 8) {
		pt->kp = k->mv_data;
		k->mv_data = twz_object_lea(obj, k->mv_data);
	} else {
		pt->kp = __c(&pt->ikey);
		memcpy(&pt->ikey, k->mv_data, k->mv_size);
	}

	pt->dp = d->mv_data;

	/* TODO: if we are replacing, free the old data...?*/
	mutex_acquire(&hdr->m);
	TXCHECK(obj, &hdr->tx);
	// Do a normal BST insert
	struct btree_node *e = NULL;
	hdr->root = __c(BSTInsert(obj, __l(obj, hdr->root), pt, &e, k, cmp));
	if(e) {
		mutex_release(&hdr->m);
		if(nt)
			*nt = e;
		oa_hdr_free(obj, &hdr->oa, __c(pt));
		return 1;
	}
	if(hdr->root == pt) {
		_clwb(&hdr->root);
		_pfence();
	}
	if(nt)
		*nt = pt;
	pt = __c(pt);

	// fix Red Black Tree violations
	fixViolation(obj, &hdr->tx, &hdr->root, &pt);
	mutex_release(&hdr->m);
	return 0;
}

int bt_insert(twzobj *obj,
  struct btree_hdr *hdr,
  struct btree_val *k,
  struct btree_val *d,
  struct btree_node **nt)
{
	return bt_insert_cmp(obj, hdr, k, d, nt, __cf_default);
}

static __inline__ unsigned long long rdtsc(void)
{
	unsigned hi, lo;
	//__asm__ __volatile__("rdtsc" : "=a"(lo), "=d"(hi));
	return ((unsigned long long)lo) | (((unsigned long long)hi) << 32);
}

static struct btree_node *__bt_leftmost(twzobj *obj, struct btree_node *n)
{
	while(n->left) {
		n = __l(obj, n->left);
	}
	return n;
}

static struct btree_node *__bt_leftmost2(twzobj *obj, struct btree_node *n, size_t *c)
{
	while(n->left) {
		(*c)++;
		n = __l(obj, n->left);
	}
	return n;
}

static struct btree_node *__bt_rightmost(twzobj *obj, struct btree_node *n)
{
	while(n->right) {
		n = __l(obj, n->right);
	}
	return n;
}
static struct btree_node *__bt_next(twzobj *obj, struct btree_hdr *hdr, struct btree_node *n)
{
	//	long long a = rdtsc();
	if(n->right) {
		struct btree_node *r = __bt_leftmost(obj, __l(obj, n->right));
		// long long b = rdtsc();
		// debug_printf(":: bt_next 1: %ld (%ld)\n", b - a, c);
		return r;
	}
	struct btree_node *p = __l(obj, n->parent);
	while(p && __c(n) == p->right) {
		n = p;
		p = __l(obj, p->parent);
	}
	//	long long b = rdtsc();
	//	debug_printf(":: bt_next 2: %ld (%ld)\n", b - a, c);
	return p;
}

static struct btree_node *__bt_prev(twzobj *obj, struct btree_hdr *hdr, struct btree_node *n)
{
	if(n->left) {
		struct btree_node *r = __bt_rightmost(obj, __l(obj, n->left));

		return r;
	}
	struct btree_node *p = __l(obj, n->parent);
	while(p && __c(n) == p->left) {
		n = p;
		p = __l(obj, p->parent);
	}
	return p;
}

static struct btree_node *__bt_sibling(twzobj *obj, struct btree_hdr *hdr, struct btree_node *node)
{
	struct btree_node *p = __l(obj, node->parent);
	if(!p)
		return NULL;
	if(__c(node) == p->left)
		return __l(obj, p->right);
	return __l(obj, p->left);
}

static void __delete_fixup2(twzobj *obj, struct btree_hdr *hdr, struct btree_node *node);
static void __delete_fixup6(twzobj *obj, struct btree_hdr *hdr, struct btree_node *node)
{
	struct btree_node *s = __bt_sibling(obj, hdr, node);
	if(!s)
		return;
	struct btree_node *p = __l(obj, node->parent);
	struct btree_node *slv = __l(obj, s->left);
	struct btree_node *srv = __l(obj, s->right);
	s->color = p->color;
	p->color = BLACK;
	if(__c(node) == p->left) {
		if(srv)
			srv->color = BLACK;
		/* rotate left (p) */
		struct btree_node *xp = __c(p);
		rotateLeft(obj, &hdr->tx, &hdr->root, &xp);
	} else {
		if(slv)
			slv->color = BLACK;
		/* rotate right (p) */
		struct btree_node *xp = __c(p);
		rotateRight(obj, &hdr->tx, &hdr->root, &xp);
	}
}

static void __delete_fixup5(twzobj *obj, struct btree_hdr *hdr, struct btree_node *node)
{
	struct btree_node *s = __bt_sibling(obj, hdr, node);
	if(!s)
		return;
	struct btree_node *p = __l(obj, node->parent);
	struct btree_node *slv = __l(obj, s->left);
	struct btree_node *srv = __l(obj, s->right);
	/* case 5 */
	if(s->color == BLACK) {
		if(__c(node) == p->left && srv && srv->color == BLACK && slv && slv->color == RED) {
			s->color = RED;
			slv->color = BLACK;
			/* rotate right (s) */
			struct btree_node *xp = __c(s);
			rotateRight(obj, &hdr->tx, &hdr->root, &xp);
		} else if(__c(node) == p->right && slv && slv->color == BLACK && srv && srv->color == RED) {
			s->color = RED;
			srv->color = BLACK;
			/* rotate left (s) */
			struct btree_node *xp = __c(s);
			rotateLeft(obj, &hdr->tx, &hdr->root, &xp);
		}
	}
	__delete_fixup6(obj, hdr, node);
}

static void __delete_fixup4(twzobj *obj, struct btree_hdr *hdr, struct btree_node *node)
{
	struct btree_node *s = __bt_sibling(obj, hdr, node);
	if(!s)
		return;
	struct btree_node *p = __l(obj, node->parent);
	struct btree_node *slv = __l(obj, s->left);
	struct btree_node *srv = __l(obj, s->right);
	if(p->color == RED && s->color == BLACK && slv && slv->color == BLACK && srv
	   && srv->color == BLACK) {
		s->color = RED;
		p->color = BLACK;
	} else {
		__delete_fixup5(obj, hdr, node);
	}
}

static void __delete_fixup3(twzobj *obj, struct btree_hdr *hdr, struct btree_node *node)
{
	struct btree_node *s = __bt_sibling(obj, hdr, node);
	if(!s)
		return;
	struct btree_node *p = __l(obj, node->parent);
	struct btree_node *slv = __l(obj, s->left);
	struct btree_node *srv = __l(obj, s->right);
	if(p->color == BLACK && s->color == BLACK && (srv && srv->color == BLACK)
	   && (slv && slv->color == BLACK)) {
		s->color = RED;
		__delete_fixup2(obj, hdr, p);
	} else {
		__delete_fixup4(obj, hdr, node);
	}
}

static void __delete_fixup2(twzobj *obj, struct btree_hdr *hdr, struct btree_node *node)
{
	if(node->parent == NULL)
		return;
	struct btree_node *s = __bt_sibling(obj, hdr, node);
	struct btree_node *p = __l(obj, node->parent);
	if(!s)
		return;
	if(s->color == RED) {
		p->color = RED;
		s->color = BLACK;
		if(__c(node) == p->left) {
			/* rotate left (p) */
			struct btree_node *xp = __c(p);
			rotateLeft(obj, &hdr->tx, &hdr->root, &xp);
		} else {
			/* rotate right (p) */
			struct btree_node *xp = __c(p);
			rotateRight(obj, &hdr->tx, &hdr->root, &xp);
		}
	}
	__delete_fixup3(obj, hdr, node);
}

static struct btree_node *__bt_delete(twzobj *obj, struct btree_hdr *hdr, struct btree_node *node)
{
	struct btree_node *ret = node, *child = NULL, *free = NULL, *fixup = NULL;
	int rcode, done = 0;

	//_doprint_tree(obj, 0, __l(obj, hdr->root));

	TXOPT_START(obj, &hdr->tx, rcode)
	{
		while(!done) {
			//	debug_printf("DEL: %p\n", __c(node));
			if(node->left && node->right) {
				/* two children case: swap a pred or succ node with us, delete that node. */
				child = __bt_next(obj, hdr, node);
				if(!child) {
					child = __bt_prev(obj, hdr, node);
				}
			} else if(!node->left && !node->right) {
				/* no children case */
				struct btree_node *next = __bt_next(obj, hdr, node);
				if(!next) {
					next = __bt_prev(obj, hdr, node);
				}
				if(node->parent) {
					struct btree_node *p = __l(obj, node->parent);
					if(__c(node) == p->left) {
						TXOPT_RECORD(&hdr->tx, &p->left);
						p->left = NULL;
					} else {
						TXOPT_RECORD(&hdr->tx, &p->right);
						p->right = NULL;
					}
				}
				if(hdr->root == __c(node)) {
					TXOPT_RECORD(&hdr->tx, &hdr->root);
					hdr->root = NULL;
				}
				free = node;
				ret = next;
				done = 1;
			} else {
				/* one child case */
				if(node->right)
					child = __bt_next(obj, hdr, node);
				else
					child = __bt_prev(obj, hdr, node);
				if(child && child->color == BLACK)
					fixup = child;
			}

			if(!done) {
				TXOPT_RECORD_TMP(&hdr->tx, &node->kp);
				TXOPT_RECORD_TMP(&hdr->tx, &node->ikey);
				TXOPT_RECORD_TMP(&hdr->tx, &node->dp);
				TXOPT_RECORD_TMP(&hdr->tx, &node->ks);
				TXOPT_RECORD_TMP(&hdr->tx, &node->ds);
				TXOPT_RECORD_TMP(&hdr->tx, &node->color);
				TX_RECORD_COMMIT(&hdr->tx);
				node->kp = child->kp;
				node->ikey = child->ikey;
				node->dp = child->dp;
				node->ks = child->ks;
				node->ds = child->ds;
				node->color = BLACK;
				if(node->ks <= 8) {
					node->kp = __c(&node->ikey);
				}
			}
			node = child;
		}

		TXOPT_COMMIT;
	}
	TXOPT_END;
	if(free) {
		oa_hdr_free(obj, &hdr->oa, __c(free));
	}
	//_doprint_tree(obj, 0, __l(obj, hdr->root));
	if(fixup) {
		__delete_fixup2(obj, hdr, fixup);
	}
	return ret;
}

struct btree_node *bt_delete(twzobj *obj, struct btree_hdr *hdr, struct btree_node *node)
{
	if(hdr->magic != BTMAGIC)
		return NULL;
	mutex_acquire(&hdr->m);
	TXCHECK(obj, &hdr->tx);
	oa_hdr_free(obj, &hdr->oa, node->dp);
	if(node->ks > 8)
		oa_hdr_free(obj, &hdr->oa, node->kp);
	struct btree_node *r = __bt_delete(obj, hdr, node);
	mutex_release(&hdr->m);
	return r;
}

/* lea: 1 */
/* can: 0 */
/* eq: 0 */
static struct btree_node *_dolookup(twzobj *obj,
  struct btree_node *root,
  struct btree_val *k,
  int (*cmp)(const struct btree_val *, const struct btree_val *))
{
	// debug_printf(":: root=%p\n", root);
	if(!root)
		return NULL;
	struct btree_val rootkey = {
		.mv_data = twz_object_lea(obj, root->kp),
		.mv_size = root->ks,
	};
	int c = -cmp(k, &rootkey);
	// debug_printf("CMP: %s %s: %d\n", (uint32_t*)rootkey.mv_data, (uint32_t*)k->mv_data, c);
	if(!c) {
		return root;
	} else if(c > 0) {
		return _dolookup(obj, __l(obj, root->left), k, cmp);
	} else {
		return _dolookup(obj, __l(obj, root->right), k, cmp);
	}
}

struct btree_node *bt_last(twzobj *obj, struct btree_hdr *hdr)
{
	if(hdr->magic != BTMAGIC) {
		return NULL;
	}
	mutex_acquire(&hdr->m);
	TXCHECK(obj, &hdr->tx);
	struct btree_node *n = __l(obj, hdr->root);
	struct btree_node *l = __bt_rightmost(obj, n);
	mutex_release(&hdr->m);
	return l;
}

struct btree_node *bt_first(twzobj *obj, struct btree_hdr *hdr)
{
	if(hdr->magic != BTMAGIC) {
		return NULL;
	}

	mutex_acquire(&hdr->m);
	TXCHECK(obj, &hdr->tx);
	struct btree_node *n = __l(obj, hdr->root);
	struct btree_node *f = __bt_leftmost(obj, n);
	mutex_release(&hdr->m);
	return f;
}

struct btree_node *bt_prev(twzobj *obj, struct btree_hdr *hdr, struct btree_node *n)
{
	if(hdr->magic != BTMAGIC) {
		return NULL;
	}
	if(!n)
		return n;
	mutex_acquire(&hdr->m);
	TXCHECK(obj, &hdr->tx);
	struct btree_node *p = __bt_prev(obj, hdr, n);
	mutex_release(&hdr->m);
	return p;
}
struct btree_node *bt_next(twzobj *obj, struct btree_hdr *hdr, struct btree_node *n)
{
	if(hdr->magic != BTMAGIC) {
		return NULL;
	}
	if(!n)
		return n;
	mutex_acquire(&hdr->m);
	TXCHECK(obj, &hdr->tx);
	struct btree_node *p = __bt_next(obj, hdr, n);
	mutex_release(&hdr->m);
	return p;
}

int bt_init(twzobj *obj, struct btree_hdr *hdr)
{
	hdr->magic = BTMAGIC;
	hdr->root = NULL;
	mutex_init(&hdr->m);
	tx_init(&hdr->tx, __BT_HDR_LOG_SZ);
	_clwb_len(hdr, sizeof(*hdr));
	_pfence();
	return oa_hdr_init(obj, &hdr->oa, 0x3000, OBJ_MAXSIZE - 0x8000);
}

struct btree_node *bt_lookup_cmp(twzobj *obj,
  struct btree_hdr *hdr,
  struct btree_val *k,
  int (*cmp)(const struct btree_val *, const struct btree_val *))
{
	if(hdr->magic != BTMAGIC) {
		return NULL;
	}
	mutex_acquire(&hdr->m);
	TXCHECK(obj, &hdr->tx);
	struct btree_node *n = _dolookup(obj, __l(obj, hdr->root), k, cmp);
	mutex_release(&hdr->m);
	return n;
}

struct btree_node *bt_lookup(twzobj *obj, struct btree_hdr *hdr, struct btree_val *k)
{
	return bt_lookup_cmp(obj, hdr, k, __cf_default);
}

int bt_node_get(twzobj *obj, struct btree_hdr *hdr, struct btree_node *n, struct btree_val *v)
{
	(void)hdr;
	v->mv_size = n->ds;
	v->mv_data = twz_object_lea(obj, n->dp);
	return 0;
}

int bt_node_getkey(twzobj *obj, struct btree_hdr *hdr, struct btree_node *n, struct btree_val *v)
{
	(void)hdr;
	v->mv_size = n->ks;
	v->mv_data = twz_object_lea(obj, n->kp);
	return 0;
}

int bt_put_cmp(twzobj *obj,
  struct btree_hdr *hdr,
  struct btree_val *k,
  struct btree_val *v,
  struct btree_node **node,
  int (*cmp)(const struct btree_val *, const struct btree_val *))
{
	void *dest_k, *vdest_k = k->mv_data;
	if(k->mv_size > 8) {
		dest_k = oa_hdr_alloc(obj, &hdr->oa, k->mv_size);
		if(!dest_k)
			return -ENOSPC;
		vdest_k = twz_object_lea(obj, dest_k);
		memcpy(vdest_k, k->mv_data, k->mv_size);
		_clwb_len(vdest_k, k->mv_size);
		k->mv_data = dest_k;
	}

	void *dest_v = NULL;
	void *vdest_v = NULL;
	if(v) {
		dest_v = oa_hdr_alloc(obj, &hdr->oa, v->mv_size);
		if(!dest_v) {
			if(k->mv_size > 8)
				oa_hdr_free(obj, &hdr->oa, dest_k);
			return -ENOSPC;
		}
		vdest_v = twz_object_lea(obj, dest_v);
		memcpy(vdest_v, v->mv_data, v->mv_size);
		_clwb_len(vdest_v, v->mv_size);
		_pfence();
		v->mv_data = dest_v;
	}

	struct btree_val nv = { .mv_data = NULL, .mv_size = 0 };
	if(!v)
		v = &nv;

	int r = bt_insert_cmp(obj, hdr, k, v, node, cmp);
	k->mv_data = vdest_k;
	v->mv_data = vdest_v;
	return r;
}

int bt_put(twzobj *obj,
  struct btree_hdr *hdr,
  struct btree_val *k,
  struct btree_val *v,
  struct btree_node **node)
{
	return bt_put_cmp(obj, hdr, k, v, node, __cf_default);
}

static void _doprint_tree(twzobj *obj, int indent, struct btree_node *root)
{
	debug_printf("%*s %p", indent, "", __c(root));
	if(!root) {
		debug_printf("\n");
		return;
	}
	struct btree_val rootkey = {
		.mv_data = twz_object_lea(obj, root->kp),
		.mv_size = root->ks,
	};
	debug_printf(" %s [", root->color == BLACK ? "BLACK" : "  RED");
	for(unsigned int i = 0; i < 16 && rootkey.mv_size < 30 && i < rootkey.mv_size; i++) {
		debug_printf("%x ", ((unsigned char *)rootkey.mv_data)[i]);
	}
	// debug_printf("%lx", *(uint64_t *)rootkey.mv_data);
	debug_printf("]\n");
	if(!root->left && !root->right)
		return;

	_doprint_tree(obj, indent + 2, __l(obj, root->right));
	_doprint_tree(obj, indent + 2, __l(obj, root->left));
}

void bt_print_tree(twzobj *obj, struct btree_hdr *hdr)
{
	mutex_acquire(&hdr->m);
	TXCHECK(obj, &hdr->tx);
	mutex_release(&hdr->m);
	_doprint_tree(obj, 0, __l(obj, hdr->root));
}
