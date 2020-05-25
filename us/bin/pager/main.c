#include <pthread.h>
#include <stdio.h>
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

	printf("Doing write!\n");
	*p = 4;
	printf("Write got %d\n", *p);
	return NULL;
}

void handle_pager_req(struct queue_hdr *hdr, struct queue_entry_pager *pqe)
{
	printf("[pager] got request for object " IDFMT "\n", IDPR(pqe->id));
	if(pqe->qe.info == PAGER_CMD_OBJECT) {
		queue_complete(hdr, (struct queue_entry *)pqe, 0);
	} else if(pqe->qe.info == PAGER_CMD_OBJECT_PAGE) {

		printf("[pager]   page request: %lx\n", pqe->page);
		//queue_complete(hdr, (struct queue_entry *)pqe, 0);
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

	struct queue_hdr *hdr = twz_object_base(&kq);
	queue_init_hdr(
	  &kq, hdr, 32, sizeof(struct queue_entry_pager), 32, sizeof(struct queue_entry_pager));

	if((r = sys_kqueue(twz_object_guid(&kq), KQ_PAGER, 0))) {
		fprintf(stderr, "failed to assign kqueue\n");
		return 1;
	}

	pthread_t pt;
	pthread_create(&pt, NULL, tm, NULL);
	while(1) {
		struct queue_entry_pager pqe;
		printf("[pager] queue_receive\n");
		int r = queue_receive(hdr, (struct queue_entry *)&pqe, 0);
		if(r == 0) {
			handle_pager_req(hdr, &pqe);
		}
	}
}
