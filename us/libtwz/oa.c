#include <stdint.h>
#include <twz/_err.h>
#include <twz/alloc.h>
#include <twz/obj.h>

/* based on https://github.com/matianfu/buddy/blob/master/buddy.c */

/* TODO: robust, persist */

#define POOLSIZE (1ul << MAX_ORDER)
#define BLOCKSIZE(i) (1ul << (i))

#define OFF(b, h) ((uintptr_t)b - (h)->start)
#define _BOF(b, i, h) (OFF(b, h) ^ (1 << (i)))
#define BOF(b, i, h) ((void *)(_BOF(b, i, h) + (h)->start))

#include <twz/debug.h>
static void *__buddy_alloc(twzobj *o, struct twzoa_header *hdr, size_t size)
{
	unsigned int i = 0, order;
	size++;

	while(BLOCKSIZE(i) < size)
		i++;
	order = i = (i < MIN_ORDER) ? MIN_ORDER : i;

	for(;; i++) {
		if(i > hdr->bdy.max_order)
			return NULL;
		if(hdr->bdy.flist[i])
			break;
	}

	void *block = hdr->bdy.flist[i];
	void *vblock = twz_object_lea(o, block);
	hdr->bdy.flist[i] = *(void **)vblock;

	while(i-- > order) {
		void *buddy = BOF(block, i, hdr);
		hdr->bdy.flist[i] = buddy;
	}
	*(((uint8_t *)vblock) - 1) = order;
	return block;
}

static void __buddy_free(twzobj *o, struct twzoa_header *hdr, void *block)
{
	void *vblock = twz_object_lea(o, block);
	// fetch order in previous byte
	int i = *((uint8_t *)(vblock - 1));

	for(;; i++) {
		// calculate buddy
		void *buddy = BOF(block, i, hdr);
		void **p = &(hdr->bdy.flist[i]);

		// find buddy in list
		while((*p != NULL) && (*p != buddy)) {
			void **vp = twz_object_lea(o, (*p));
			p = (void **)*vp;
		}

		// not found, insert into list
		if(*p != buddy) {
			*(void **)vblock = hdr->bdy.flist[i];
			hdr->bdy.flist[i] = block;
			return;
		}
		// found, merged block starts from the lower one
		block = (block < buddy) ? block : buddy;
		// remove buddy out of list
		void **vp = twz_object_lea(o, (*p));
		*p = (void *)*vp;
	}
}

void oa_hdr_free(twzobj *obj, struct twzoa_header *hdr, void *p)
{
	mutex_acquire(&hdr->m);
	__buddy_free(obj, hdr, p);
	mutex_release(&hdr->m);
}

void *oa_hdr_alloc(twzobj *obj, struct twzoa_header *hdr, size_t s)
{
	mutex_acquire(&hdr->m);
	void *r = __buddy_alloc(obj, hdr, s);
	mutex_release(&hdr->m);
	return r;
}

int oa_hdr_init(twzobj *obj, struct twzoa_header *h, size_t start, size_t end)
{
	(void)obj;
	start += 16;
	h->start = start;
	h->end = end;
	h->bdy.max_order = 0;
	mutex_init(&h->m);
	memset(h->bdy.flist, 0, sizeof(h->bdy.flist));
	while(BLOCKSIZE(h->bdy.max_order + 1) < end - start && h->bdy.max_order < MAX_ORDER) {
		h->bdy.max_order++;
	}
	if(h->bdy.max_order <= MIN_ORDER)
		return -ENOSPC;
	h->bdy.flist[h->bdy.max_order] = (void *)start;
	return 0;
}
