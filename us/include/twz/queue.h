#pragma once

#include <twz/_queue.h>

#define QUEUE_NONBLOCK 1

#ifdef __cplusplus
extern "C" {
#endif
int queue_submit(twzobj *, struct queue_entry *qe, int flags);
int queue_complete(twzobj *, struct queue_entry *qe, int flags);
int queue_get_finished(twzobj *, struct queue_entry *qe, int flags);
int queue_receive(twzobj *, struct queue_entry *qe, int flags);
int queue_init_hdr(twzobj *obj, size_t sqlen, size_t sqstride, size_t cqlen, size_t cqstride);
#ifdef __cplusplus
}
#endif
