#include <cstdio>
#include <twz/obj.h>
#include <twz/objctl.h>
#include <twz/queue.h>
#include <unistd.h>

#include <thread>
#include <twz/driver/queue.h>

twzobj txqueue_obj, rxqueue_obj;

void consumer()
{
	twzobj pdo;
	int ok = 0;
	while(1) {
		struct queue_entry_packet packet;
		queue_receive(&rxqueue_obj, (struct queue_entry *)&packet, 0);
		fprintf(stderr, "net got packet!\n");

		if(!ok) {
			twz_object_init_guid(&pdo, packet.objid, FE_READ);
			ok = 1;
		}

		size_t offset = (packet.pdata % OBJ_MAXSIZE) - OBJ_NULLPAGE_SIZE;
		void *pd = twz_object_lea(&pdo, (void *)offset);
		// fprintf(stderr, ":: %lx %p\n", offset, pd);

		//	char buf[packet.len + 1];
		//	memset(buf, 0, sizeof(buf));
		//	memcpy(buf, pd, packet.len);

		fprintf(stderr, ":: packet: %s\n", (char *)pd);

		queue_complete(&rxqueue_obj, (struct queue_entry *)&packet, 0);
	}
}

int main(int argc, char **argv)
{
	if(argc < 3) {
		fprintf(stderr, "usage: net txqueue rxqueue\n");
		return 1;
	}
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

	std::thread thr(consumer);

	void *d = twz_object_base(&buf_obj);
	sprintf((char *)d, "              this is a test from net!\n");
	struct queue_entry_packet p;
	p.objid = twz_object_guid(&buf_obj);
	p.pdata = buf_pin;
	p.len = 62;

	p.cmd = PACKET_CMD_SEND;
	p.stat = 0;

	p.qe.info = 123;

	while(1) {
		usleep(100000);
		fprintf(stderr, "submiting: %d\n", p.qe.info);
		queue_submit(&txqueue_obj, (struct queue_entry *)&p, 0);
		fprintf(stderr, "submitted: %d\n", p.qe.info);

		queue_get_finished(&txqueue_obj, (struct queue_entry *)&p, 0);
		fprintf(stderr, "completed: %d\n", p.qe.info);
	}

	for(;;)
		usleep(10000);
}
