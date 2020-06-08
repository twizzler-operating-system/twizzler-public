#include <cstdio>
#include <twz/debug.h>
#include <twz/name.h>
#include <twz/obj.h>
#include <twz/objctl.h>
#include <twz/queue.h>
#include <twz/sys.h>

#include <twz/driver/queue.h>

#include <condition_variable>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vector>

twzobj kq;

std::queue<struct queue_entry_pager *> incoming_reqs;
std::mutex incoming_lock;
std::condition_variable incoming_cv;

struct sb {
	char magic[16];
	objid_t nameroot;
	uint64_t hashlen;
};

struct bucket {
	objid_t id;
	uint64_t pgnum;
	uint64_t next;
	uint64_t chainpage;
	uint64_t datapage;
};

static size_t get_bucket_num(objid_t id, size_t pg, size_t htlen)
{
	return (id ^ ((objid_t)pg << (pg % 31))) % htlen;
}

class device
{
  public:
	device(twzobj *queue)
	{
		req_queue = queue;
		int r;
		if((r = twz_object_new(
		      &tmpdata, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_VOLATILE))
		   < 0) {
			throw "failed to create tmpdata object";
		}

		r = twz_object_pin(&tmpdata, &tmpdata_pin, 0);
		if(r) {
			throw "failed to pin";
		}
	}
	twzobj tmpdata;
	uintptr_t tmpdata_pin;
	std::unordered_map<uint64_t, struct bucket *> chain_map;
	twzobj *req_queue;
	struct sb *sb;
	size_t offset;

	void get_sb()
	{
		struct queue_entry_bio bio;
		bio.qe.info = 0;
		bio.tmpobjid = twz_object_guid(&tmpdata);
		bio.blockid = 0;
		bio.linaddr = tmpdata_pin;

		printf("[pager] submitting get_sb\n");
		queue_submit(req_queue, (struct queue_entry *)&bio, 0);
		queue_get_finished(req_queue, (struct queue_entry *)&bio, 0);

		if(strcmp((char *)twz_object_base(&tmpdata), "TWIZZLER DEVICE")) {
			fprintf(stderr, "[pager] tried to open invalid device sb\n");
		}

		sb = (struct sb *)twz_object_base(&tmpdata);
		printf("[pager] opened device with nameroot = " IDFMT ", hashlen = %ld\n",
		  IDPR(sb->nameroot),
		  sb->hashlen);
		size_t nrhtpgs =
		  (((sb->hashlen * sizeof(struct bucket)) + (0x1000 - 1)) & ~(0x1000 - 1)) / 0x1000;
		printf("[pager] reading hash table (%ld pages)\n", nrhtpgs);
		for(size_t i = 0; i < nrhtpgs; i++) {
			bio.qe.info = 0;
			bio.tmpobjid = twz_object_guid(&tmpdata);
			bio.blockid = 1 + i;
			bio.linaddr = tmpdata_pin + 0x1000 * (1 + i);

			queue_submit(req_queue, (struct queue_entry *)&bio, 0);
			queue_get_finished(req_queue, (struct queue_entry *)&bio, 0);
		}
		offset = nrhtpgs + 1;
		chain_map[1] = (struct bucket *)((char *)twz_object_base(&tmpdata) + 0x1000);
		printf("[pager] mounting namespace\n");
		twz_name_assign_namespace(sb->nameroot, (char const *)"/storage");
		//	printf("ok\n");
	}

	struct bucket *get_bucket(uint64_t chainpage, size_t bn)
	{
		if(chain_map.find(chainpage) == chain_map.end()) {
			printf("[pager] reading chain page %ld -> %lx\n", chainpage, offset * 0x1000);
			struct queue_entry_bio bio;
			bio.qe.info = 0xffffffff;
			bio.tmpobjid = twz_object_guid(&tmpdata);
			bio.blockid = chainpage;
			bio.linaddr = tmpdata_pin + offset * 0x1000;

			queue_submit(req_queue, (struct queue_entry *)&bio, 0);
			queue_get_finished(req_queue, (struct queue_entry *)&bio, 0);
			chain_map[chainpage] =
			  (struct bucket *)((char *)twz_object_base(&tmpdata) + offset * 0x1000);
			offset++;
		}
		struct bucket *b = chain_map[chainpage];
		return &b[bn];
	}

	ssize_t page_lookup(objid_t id, size_t pgnr)
	{
		printf("page lookup " IDFMT " page %ld\n", IDPR(id), pgnr);
		struct bucket *b = get_bucket(1, get_bucket_num(id, pgnr, sb->hashlen));

		printf("bucket: " IDFMT " %ld :: %ld %ld\n", IDPR(b->id), b->pgnum, b->chainpage, b->next);
		while(b->id != 0 && (b->id != id || b->pgnum != pgnr) && b->chainpage != 0) {
			b = get_bucket(b->chainpage, b->next);

			printf(
			  "bucket: " IDFMT " %ld :: %ld %ld\n", IDPR(b->id), b->pgnum, b->chainpage, b->next);
		}

		if(b->id != id || b->pgnum != pgnr)
			return -1;
		else
			return b->datapage;
	}
};

struct device *nvme_dev;

void tm()
{
	printf("hello world from pager thread\n");

	//	7ca09742a7f167ec:9d3e739c2cf7a9e5
	//	baa8a8f8dc8eff5d:cf280ad70a6600e2
	//	//d8818bc65aac0192:213f58281bf6df12
	objid_t id = ((objid_t)0xd8818bc65aac0192ul << 64) | 0x213f58281bf6df12ul;
	twzobj obj;
	twz_object_init_guid(&obj, id, FE_READ | FE_WRITE);

	int *p = (int *)twz_object_base(&obj);

	debug_printf("[pager] p = %p\n", p);
	printf("Doing read!\n");
	printf("::: %d\n", *p);
	printf("Doing write!\n");
	*p = 4;
	printf("Write got %d\n", *p);
}

void handle_pager_req(twzobj *qobj, struct queue_entry_pager *pqe, device *dev)
{
	printf("[pager] got request for object " IDFMT "\n", IDPR(pqe->id));
	if(pqe->cmd == PAGER_CMD_OBJECT) {
		ssize_t r = dev->page_lookup(pqe->id, 0);
		printf("[pager] page_lookup returned %ld\n", r);
		pqe->result = r < 0 ? PAGER_RESULT_ERROR : PAGER_RESULT_DONE;
		queue_complete(qobj, (struct queue_entry *)pqe, 0);
		printf("[pager] complete!\n");
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

		ssize_t r = dev->page_lookup(pqe->id, pqe->page);
		printf("[pager] page_lookup returned %ld\n", r);
		if(r < 0) {
			pqe->result = PAGER_RESULT_DONE;
			queue_complete(qobj, (struct queue_entry *)pqe, 0);
			return;
		}

		struct queue_entry_bio bio;
		bio.qe.info = 0;
		bio.tmpobjid = pqe->tmpobjid;
		bio.blockid = r;
		bio.linaddr = pqe->linaddr;

		printf("[pager] forwarding request to nvme\n");
		queue_submit(dev->req_queue, (struct queue_entry *)&bio, 0);
		printf("[pager] submitted!\n");
		queue_get_finished(dev->req_queue, (struct queue_entry *)&bio, 0);
		printf("[pager] heard back from nvme: %d\n", bio.result);
		pqe->result = PAGER_RESULT_DONE;
		queue_complete(qobj, (struct queue_entry *)pqe, 0);
	}
}

//		handle_pager_req(&kq, pqe, &nvme_queue);
void foo()
{
	while(1) {
		struct queue_entry_pager *pqe = NULL;
		{
			std::unique_lock<std::mutex> lck(incoming_lock);
			if(incoming_reqs.empty()) {
				incoming_cv.wait(lck);
			} else {
				pqe = incoming_reqs.front();
				incoming_reqs.pop();
			}
		}

		if(pqe) {
			printf("[pager] handler thread got %d\n", pqe->qe.info);
			handle_pager_req(&kq, pqe, nvme_dev);
		}
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

	queue_init_hdr(&kq, 5, sizeof(struct queue_entry_pager), 5, sizeof(struct queue_entry_pager));

	if((r = sys_kqueue(twz_object_guid(&kq), KQ_PAGER, 0))) {
		fprintf(stderr, "failed to assign kqueue\n");
		return 1;
	}

	twzobj *nvme_queue = (twzobj *)malloc(sizeof(&nvme_queue));
	if(twz_object_init_name(nvme_queue, "/dev/nvme-queue", FE_READ | FE_WRITE)) {
		fprintf(stderr, "failed to open nvme queue\n");
		return 1;
	}

	nvme_dev = new device(nvme_queue);

	nvme_dev->get_sb();

	std::thread thr(foo);
	std::thread thr2(tm);

	while(1) {
		queue_entry_pager *pqe = new queue_entry_pager;
		int r = queue_receive(&kq, (struct queue_entry *)pqe, 0);
		printf("[pager]: got request %d\n", pqe->qe.info);
		if(r == 0) {
			std::unique_lock<std::mutex> lck(incoming_lock);
			incoming_reqs.push(pqe);
			incoming_cv.notify_all();
		}
	}
}
