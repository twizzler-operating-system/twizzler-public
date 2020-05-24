#include <errno.h>
#include <twz/obj.h>
#include <twz/queue.h>

int queue_submit(struct queue_hdr *hdr, struct queue_entry *qe, int flags)
{
	return queue_sub_enqueue(hdr, SUBQUEUE_SUBM, qe, !!(flags & QUEUE_NONBLOCK));
}

int queue_complete(struct queue_hdr *hdr, struct queue_entry *qe, int flags)
{
	return queue_sub_enqueue(hdr, SUBQUEUE_CMPL, qe, !!(flags & QUEUE_NONBLOCK));
}

int queue_receive(struct queue_hdr *hdr, struct queue_entry *qe, int flags)
{
	return queue_sub_dequeue(hdr, SUBQUEUE_SUBM, qe, !!(flags & QUEUE_NONBLOCK));
}

int queue_get_finished(struct queue_hdr *hdr, struct queue_entry *qe, int flags)
{
	return queue_sub_dequeue(hdr, SUBQUEUE_CMPL, qe, !!(flags & QUEUE_NONBLOCK));
}

int queue_init_hdr(twzobj *obj,
  struct queue_hdr *hdr,
  size_t sqlen,
  size_t sqstride,
  size_t cqlen,
  size_t cqstride)
{
	if(sqstride < sizeof(struct queue_entry))
		sqstride = sizeof(struct queue_entry);
	sqstride = (sqstride + 7) & ~7;
	if(cqstride < sizeof(struct queue_entry))
		cqstride = sizeof(struct queue_entry);
	if(sqlen * sqstride + sizeof(struct queue_hdr) + cqlen * cqstride >= OBJ_TOPDATA) {
		return -EINVAL;
	}
	cqstride = (cqstride + 7) & ~7;
	memset(hdr, 0, sizeof(*hdr));
	hdr->subqueue[SUBQUEUE_SUBM].length = sqlen;
	hdr->subqueue[SUBQUEUE_CMPL].length = cqlen;
	hdr->subqueue[SUBQUEUE_SUBM].stride = sqstride;
	hdr->subqueue[SUBQUEUE_CMPL].stride = cqstride;
	hdr->subqueue[SUBQUEUE_SUBM].queue = (void *)twz_ptr_local(hdr + 1);
	hdr->subqueue[SUBQUEUE_CMPL].queue =
	  (void *)twz_ptr_local((char *)(hdr + 1) + sqlen * sqstride);
	return 0;
}
