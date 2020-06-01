#include <errno.h>
#include <twz/obj.h>
#include <twz/queue.h>

int queue_submit(twzobj *obj, struct queue_entry *qe, int flags)
{
	return queue_sub_enqueue(
	  obj, twz_object_base(obj), SUBQUEUE_SUBM, qe, !!(flags & QUEUE_NONBLOCK));
}

int queue_complete(twzobj *obj, struct queue_entry *qe, int flags)
{
	return queue_sub_enqueue(
	  obj, twz_object_base(obj), SUBQUEUE_CMPL, qe, !!(flags & QUEUE_NONBLOCK));
}

int queue_receive(twzobj *obj, struct queue_entry *qe, int flags)
{
	return queue_sub_dequeue(
	  obj, twz_object_base(obj), SUBQUEUE_SUBM, qe, !!(flags & QUEUE_NONBLOCK));
}

int queue_get_finished(twzobj *obj, struct queue_entry *qe, int flags)
{
	return queue_sub_dequeue(
	  obj, twz_object_base(obj), SUBQUEUE_CMPL, qe, !!(flags & QUEUE_NONBLOCK));
}

int queue_init_hdr(twzobj *obj, size_t sqlen, size_t sqstride, size_t cqlen, size_t cqstride)
{
	struct queue_hdr *hdr = twz_object_base(obj);
	if(sqstride < sizeof(struct queue_entry))
		sqstride = sizeof(struct queue_entry);
	sqstride = (sqstride + 7) & ~7;
	if(cqstride < sizeof(struct queue_entry))
		cqstride = sizeof(struct queue_entry);
	if((1ul << sqlen) * sqstride + sizeof(struct queue_hdr) + (1ul << cqlen) * cqstride
	   >= OBJ_TOPDATA) {
		return -EINVAL;
	}
	cqstride = (cqstride + 7) & ~7;
	memset(hdr, 0, sizeof(*hdr));
	hdr->subqueue[SUBQUEUE_SUBM].l2length = sqlen;
	hdr->subqueue[SUBQUEUE_CMPL].l2length = cqlen;
	hdr->subqueue[SUBQUEUE_SUBM].stride = sqstride;
	hdr->subqueue[SUBQUEUE_CMPL].stride = cqstride;
	hdr->subqueue[SUBQUEUE_SUBM].queue = (void *)twz_ptr_local(hdr + 1);
	hdr->subqueue[SUBQUEUE_CMPL].queue =
	  (void *)twz_ptr_local((char *)(hdr + 1) + (1ul << sqlen) * sqstride);
	return 0;
}
