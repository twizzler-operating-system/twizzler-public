#pragma once

#include <twz/_queue.h>

#define QUEUE_NONBLOCK 1

int queue_submit(struct queue_hdr *hdr, struct queue_entry *qe, int flags);
int queue_complete(struct queue_hdr *hdr, struct queue_entry *qe, int flags);
int queue_get_finished(struct queue_hdr *hdr, struct queue_entry *qe, int flags);
int queue_receive(struct queue_hdr *hdr, struct queue_entry *qe, int flags);
int queue_init_hdr(twzobj *obj,
  struct queue_hdr *hdr,
  size_t sqlen,
  size_t sqstride,
  size_t cqlen,
  size_t cqstride);
