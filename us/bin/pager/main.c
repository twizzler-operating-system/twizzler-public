#include <pthread.h>
#include <stdio.h>
#include <twz/debug.h>
#include <twz/obj.h>
#include <twz/queue.h>
#include <twz/sys.h>

#include <twz/driver/queue.h>

twzobj kq;

#include "test.h"

void *tm(void *a)
{
	(void)a;
	printf("hello world from pager thread\n");

	objid_t id = 0x12345678abcdef;
	twzobj obj;
	twz_object_init_guid(&obj, id, FE_READ | FE_WRITE);

	int *p = twz_object_base(&obj);

	debug_printf("[pager] p = %p\n", p);
	printf("Doing read!\n");
	printf("::: %d\n", *p);
	printf("Doing write!\n");
	*p = 4;
	printf("Write got %d\n", *p);
	return NULL;
}

void handle_pager_req(twzobj *qobj, struct queue_entry_pager *pqe, twzobj *nvme_queue)
{
	printf("[pager] got request for object " IDFMT "\n", IDPR(pqe->id));
	if(pqe->cmd == PAGER_CMD_OBJECT) {
		queue_complete(qobj, (struct queue_entry *)pqe, 0);
	} else if(pqe->cmd == PAGER_CMD_OBJECT_PAGE) {
		printf("[pager]   page request: %lx -> %lx\n", pqe->page, pqe->linaddr);
		// pqe->result = PAGER_RESULT_ZERO;
		// pqe->result = PAGER_RESULT_COPY;
		/*	twzobj o;
		    twz_object_new(&o, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE);
		    int *p = twz_object_base(&o);
		    *p = 0x1234;
		    pqe->page = 1;
		    pqe->id = twz_object_guid(&o);
		    */

		struct queue_entry_bio bio = {
			.qe = 0,
			.blockid = 0,
			.linaddr = pqe->linaddr,
		};

		printf("[pager] forwarding request to nvme\n");
		queue_submit(nvme_queue, (struct queue_entry *)&bio, 0);
		printf("[pager] submitted!\n");
		queue_get_finished(nvme_queue, (struct queue_entry *)&bio, 0);
		printf("[pager] heard back from nvme: %d\n", bio.result);
		pqe->result = PAGER_RESULT_DONE;
		queue_complete(qobj, (struct queue_entry *)pqe, 0);
	}
}

int main()
{
	printf("hello from pager\n");

	int r;
	if((r = twz_object_new(&kq, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_VOLATILE))
	   < 0) {
		fprintf(stderr, "failed to create kq object\n");
		return 1;
	}

	queue_init_hdr(&kq, 32, sizeof(struct queue_entry_pager), 32, sizeof(struct queue_entry_pager));

	if((r = sys_kqueue(twz_object_guid(&kq), KQ_PAGER, 0))) {
		fprintf(stderr, "failed to assign kqueue\n");
		return 1;
	}

	pthread_t pt;
	pthread_create(&pt, NULL, tm, NULL);

	twzobj nvme_queue;
	if(twz_object_init_name(&nvme_queue, "/dev/nvme-queue", FE_READ | FE_WRITE)) {
		fprintf(stderr, "failed to open nvme queue\n");
		return 1;
	}

	while(1) {
		struct queue_entry_pager pqe;
		printf("[pager] queue_receive\n");
		int r = queue_receive(&kq, (struct queue_entry *)&pqe, 0);
		if(r == 0) {
			handle_pager_req(&kq, &pqe, &nvme_queue);
		}
	}
}
