#include <twz/obj.h>
#include <twz/queue.h>

#include <unistd.h>

#include <cstdio>
#include <vector>

struct packet_queue_entry {
	struct queue_entry qe;
	void *ptr;
};

#include <time.h>

void timespec_diff(struct timespec *start, struct timespec *stop, struct timespec *result)
{
	if((stop->tv_nsec - start->tv_nsec) < 0) {
		result->tv_sec = stop->tv_sec - start->tv_sec - 1;
		result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
	} else {
		result->tv_sec = stop->tv_sec - start->tv_sec;
		result->tv_nsec = stop->tv_nsec - start->tv_nsec;
	}
}

void consumer(twzobj *qobj)
{
	size_t count = 0;
	struct timespec start, end, diff;
	printf("start consumer\n");
	clock_gettime(CLOCK_MONOTONIC, &start);
	while(1) {
		struct packet_queue_entry pqe;
		queue_receive(qobj, (struct queue_entry *)&pqe, 0);
		//	printf("consumer got %d: %p\n", pqe.qe.info, pqe.ptr);
		char *packet_data = twz_object_lea(qobj, (char *)pqe.ptr);
		//	printf("   read packet data: %s\n", packet_data);

		count++;
		if(count % 1000000 == 0) {
			clock_gettime(CLOCK_MONOTONIC, &end);
			timespec_diff(&start, &end, &diff);
			double seconds = diff.tv_sec + (double)diff.tv_nsec / 1000000000.f;
			printf("processed %ld packets in %4.4lf seconds (%4.4lf p/s)\n",
			  count,
			  seconds,
			  (double)count / seconds);
			count = 0;
			clock_gettime(CLOCK_MONOTONIC, &start);
		}

		// queue_complete(qobj, (struct queue_entry *)&pqe, 0);
	}
}

/* this code is for ensuring that every packet we send has a unique ID, so that when we get back the
 * completion we know which packet was sent */
static uint32_t counter = 0;
static std::vector<uint32_t> info_list;
static uint32_t get_new_info()
{
	if(info_list.size() == 0) {
		return ++counter;
	}
	uint32_t info = info_list.back();
	info_list.pop_back();
	return info;
}

static void release_info(uint32_t info)
{
	info_list.push_back(info);
}

/* some example code for making a packet. We have a data object, and we're choosing pages to fill
 * with data */

static void *make_packet(twzobj *data_obj)
{
	static void *p = NULL;
	if(!p) {
		p = twz_object_base(data_obj);
	} else {
		//	p = (void *)((char *)p + 0x1000);
	}

	static int packet_num = 0;
	// sprintf((char *)p, "Hey! This is some example packet data! Packet #%d\n", packet_num++);

	return p;
}

int main()
{
	/* make a queue object and an object to hold packet data */
	twzobj qo, data_obj;
	twz_object_new(&qo, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE);
	twz_object_new(&data_obj, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE);

	/* init the queue object, here we have 32 queue entries */
	queue_init_hdr(
	  &qo, 1 << 22, sizeof(struct packet_queue_entry), 32, sizeof(struct packet_queue_entry));

	/* start the consumer... */
	if(!fork()) {
		consumer(&qo);
	}

	struct packet_queue_entry pqe;
	size_t count = 0;
	struct timespec start, end, diff;
	clock_gettime(CLOCK_MONOTONIC, &start);
	while(1) {
		/* get a unique ID (unique only for outstanding requests; it can be reused) */
		uint32_t info = get_new_info();

		/* store a pointer to some packet data */
		pqe.ptr = twz_ptr_swizzle(&qo, make_packet(&data_obj), FE_READ);

		/* submit the packet! */
		queue_submit(&qo, (struct queue_entry *)&pqe, 0);

		count++;
		if(count % 1000000 == 0) {
			clock_gettime(CLOCK_MONOTONIC, &end);
			timespec_diff(&start, &end, &diff);
			double seconds = diff.tv_sec + (double)diff.tv_nsec / 1000000000.f;
			printf("submitted %ld packets in %4.4lf seconds (%4.4lf p/s)\n",
			  count,
			  seconds,
			  (double)count / seconds);
			count = 0;
			clock_gettime(CLOCK_MONOTONIC, &start);
		}

		//	printf("submitted %d\n", pqe.qe.info);

		/* wait for a completion */
		// queue_get_finished(&qo, (struct queue_entry *)&pqe, 0);
		//	printf("got finished %d\n", pqe.qe.info);

		/* allow this ID to be reused */
		release_info(pqe.qe.info);
	}
}
