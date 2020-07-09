#include <cstdio>
#include <twz/obj.h>
#include <twz/objctl.h>
#include <twz/queue.h>

#include <twz/driver/queue.h>

twzobj txqueue_obj, rxqueue_obj;

int main(int arg, char **argv)
{
	int r;
	r = twz_object_init_name(&txqueue_obj, argv[1], FE_READ | FE_WRITE);
	if(r) {
		fprintf(stderr, "e1000: failed to open txqueue\n");
		return 1;
	}

	r = twz_object_init_name(&rxqueue_obj, argv[2], FE_READ | FE_WRITE);
	if(r) {
		fprintf(stderr, "e1000: failed to open rxqueue\n");
		return 1;
	}

	fprintf(stderr, "NET TESTING\n");

	uint64_t buf_pin;
	twzobj buf_obj;
	if(twz_object_new(&buf_obj, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE))
		return 1;
	r = twz_object_pin(&buf_obj, &buf_pin, 0);
	if(r)
		return 1;

	void *d = twz_object_base(&buf_obj);
	sprintf((char *)d, "              this is a test from net!");
	struct queue_entry_packet p;
	p.objid = twz_object_guid(&buf_obj);
	p.pdata = buf_pin;
	p.len = 128;

	p.cmd = PACKET_CMD_SEND;
	p.stat = 0;

	p.qe.info = 123;

	fprintf(stderr, "submiting: %d\n", p.qe.info);
	queue_submit(&txqueue_obj, (struct queue_entry *)&p, 0);
	fprintf(stderr, "submitted: %d\n", p.qe.info);

	queue_get_finished(&txqueue_obj, (struct queue_entry *)&p, 0);
	fprintf(stderr, "completed: %d\n", p.qe.info);
}
