#pragma once

#define __QUEUE_TYPES_ONLY
#include <twz/_queue.h>

struct queue_entry_pager {
	struct queue_entry qe;
	objid_t id;
	objid_t tmpobjid;
	objid_t reqthread;
	uint64_t linaddr;
	uint64_t page;
	uint32_t cmd;
	uint16_t result;
	uint16_t pad;
};

#define PAGER_RESULT_DONE 0
#define PAGER_RESULT_ERROR 1
#define PAGER_RESULT_ZERO 2
#define PAGER_RESULT_COPY 3

#define PAGER_CMD_OBJECT 1
#define PAGER_CMD_OBJECT_PAGE 2

enum bio_result {
	BIO_RESULT_OK,
	BIO_RESULT_ERR,
};

struct queue_entry_bio {
	struct queue_entry qe;
	objid_t tmpobjid;
	uint64_t linaddr;
	uint64_t blockid;
	int result;
	int pad;
};

#define PACKET_CMD_SEND 0

struct queue_entry_packet {
	struct queue_entry qe;
	objid_t objid;
	uint64_t pdata;
	uint32_t len;
	uint16_t cmd;
	uint16_t stat;
};
