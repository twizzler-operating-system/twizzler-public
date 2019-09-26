#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <twz/name.h>
#include <twz/obj.h>
#include <twz/sys.h>

#include <twz/debug.h>
#include <twz/driver/device.h>
#include <twz/driver/pcie.h>

#include "nvme.h"

static void nvme_cmd_init_identify(struct nvme_cmd *cmd, uint8_t cns, uint8_t nsid, uint64_t addr)
{
	memset(cmd, 0, sizeof(*cmd));
	cmd->hdr.cdw0 = NVME_CMD_SDW0_OP(NVME_ADMIN_OP_IDENTIFY);
	cmd->hdr.nsid = nsid;
	cmd->hdr.dptr.prpp[0].addr = addr;
	cmd->cmd_dword_10[0] = cns;
}
void nvmeq_init(struct nvme_queue *q,
  struct nvme_cmd *sentries,
  struct nvme_cmp *centries,
  uint32_t count,
  uint32_t *head_db,
  uint32_t *tail_db)
{
	q->subq.entries = sentries;
	q->cmpq.entries = centries;
	q->sps = calloc(count, sizeof(uint64_t) * count);
	q->count = count;
	q->cmpq.head_doorbell = head_db;
	q->subq.tail_doorbell = tail_db;
	q->subq.head = q->subq.tail = 0;
	q->cmpq.head = q->cmpq.tail = 0;
	q->subq.phase = true;
	q->cmpq.phase = true;
}

#include <assert.h>
#include <stdatomic.h>
uint16_t nvmeq_submit_cmd(struct nvme_queue *q, struct nvme_cmd *cmd)
{
	size_t index = q->subq.tail;
	cmd->hdr.cdw0 |= (uint32_t)(index << 16);
	q->subq.entries[q->subq.tail] = *cmd;
	atomic_thread_fence(memory_order_acq_rel);

	size_t new_tail = (q->subq.tail + 1) & (q->count - 1);
	bool wr = new_tail < q->subq.tail;
	q->subq.tail = new_tail;
	*q->subq.tail_doorbell = q->subq.tail;
	q->subq.phase ^= wr;
	return (uint16_t)index;
}

void *nvme_co_get_regs(struct object *co)
{
	struct pcie_function_header *hdr = twz_device_getds(co);
	return twz_ptr_lea(co, (void *)hdr->bars[0]);
}

uint32_t nvme_reg_read32(struct nvme_controller *nc, int r)
{
	void *regs = nvme_co_get_regs(&nc->co);
	return *(volatile uint32_t *)((char *)regs + r);
}

uint64_t nvme_reg_read64(struct nvme_controller *nc, int r)
{
	void *regs = nvme_co_get_regs(&nc->co);
	return *(volatile uint64_t *)((char *)regs + r);
}

void nvme_reg_write32(struct nvme_controller *nc, int r, uint32_t v)
{
	void *regs = nvme_co_get_regs(&nc->co);
	*(volatile uint32_t *)((char *)regs + r) = v;
	asm volatile("sfence;" ::: "memory");
}

void nvme_reg_write64(struct nvme_controller *nc, int r, uint64_t v)
{
	void *regs = nvme_co_get_regs(&nc->co);
	*(volatile uint64_t *)((char *)regs + r) = v;
	asm volatile("sfence;" ::: "memory");
}

#include <twz/driver/msi.h>
int nvmec_pcie_init(struct nvme_controller *nc)
{
	struct pcie_function_header *hdr = twz_device_getds(&nc->co);
	struct device_repr *repr = twz_device_getrepr(&nc->co);
	struct pcie_config_space *space = twz_ptr_lea(&nc->co, hdr->space);
	/* bus-master enable, memory space enable. We can do interrupt disable too, since we'll be using
	 * MSI */
	space->header.command =
	  COMMAND_MEMORYSPACE | COMMAND_BUSMASTER | COMMAND_INTDISABLE | COMMAND_SERRENABLE;
	/* allocate an interrupt vector */
	struct sys_kaction_args args = {
		.id = twz_object_id(&nc->co),
		.cmd = KACTION_CMD_DEVICE_SETUP_INTERRUPTS,
		.arg = 1,
		.flags = KACTION_VALID,
	};
	int r;
	if((r = sys_kaction(1, &args)) < 0) {
		fprintf(stderr, "kaction: %d\n", r);
		return r;
	}
	if(args.result) {
		fprintf(stderr, "kaction-result: %ld\n", args.result);
		return r;
	}
	fprintf(stderr, "[nvme] allocated vector %d for interrupts\n", repr->interrupts[0].vec);

	/* try to use MSI-X, but fall back to MSI if not available */
	union pcie_capability_ptr cp;
	nc->msix = pcief_capability_get(hdr, PCIE_MSIX_CAPABILITY_ID, &cp);
	if(!nc->msix && !pcief_capability_get(hdr, PCIE_MSI_CAPABILITY_ID, &cp)) {
		fprintf(stderr, "[nvme] no interrupt generation method supported\n");
		return -ENOTSUP;
	}

	if(nc->msix) {
		volatile struct pcie_msix_capability *msix = cp.msix;
		fprintf(
		  stderr, "[nvme] table sz = %d; tob = %x\n", msix->table_size, msix->table_offset_bir);
		uint8_t bir = msix->table_offset_bir & 0x7;
		if(bir != 0 && bir != 4 && bir != 5) {
			fprintf(stderr, "[nvme] invalid BIR in table offset\n");
			return -EINVAL;
		}
		uint32_t off = msix->table_offset_bir & ~0x7;
		volatile struct pcie_msix_table_entry *tbl =
		  twz_ptr_lea(&nc->co, (void *)((long)hdr->bars[bir] + off));
		fprintf(stderr, "[nvme] MSIx: %p %p %d\n", hdr->bars[bir], tbl, off);
		tbl->data = device_msi_data(repr->interrupts[0].vec, MSI_LEVEL);
		tbl->addr = device_msi_addr(0);
		tbl->ctl = 0;
		msix->fn_mask = 0;
		msix->msix_enable = 1;
	} else {
		fprintf(stderr, "[nvme] TODO: not implemented: MSI (not MSI-X support)\n");
		return -ENOTSUP;
	}
	return 0;
}

int nvmec_reset(struct nvme_controller *nc)
{
	/* cause a controller reset */
	nvme_reg_write32(nc, NVME_REG_CC, 0);
	/* wait for RDY to clear, indicating reset completed */
	while(nvme_reg_read32(nc, NVME_REG_CSTS) & NVME_CSTS_RDY)
		asm("pause");
	return 0;
}

int nvmec_wait_for_ready(struct nvme_controller *nc)
{
	/* TODO: timeout */
	uint32_t status;
	while(!((status = nvme_reg_read32(nc, NVME_REG_CSTS)) & (NVME_CSTS_RDY | NVME_CSTS_CFS)))
		asm("pause");

	if(status & NVME_CSTS_CFS) {
		fprintf(stderr, "[nvme] fatal controller status\n");
		return -1;
	}

	return 0;
}

#define NVME_ASQS 1024
#define NVME_ACQS 1024

#define LOG2(X) ((unsigned)(8 * sizeof(unsigned long long) - __builtin_clzll((X)) - 1))

uint32_t *nvmec_get_doorbell(struct nvme_controller *nc, uint32_t dnr, bool tail)
{
	void *regs = nvme_co_get_regs(&nc->co);
	return (uint32_t *)((char *)regs
	                    + (tail ? NVME_REG_SQnTDBL(dnr, nc->dstride)
	                            : NVME_REG_CQnHDBL(dnr, nc->dstride)));
}

#include <twz/driver/device.h>
int nvmec_init(struct nvme_controller *nc)
{
	uint64_t caps = nvme_reg_read64(nc, NVME_REG_CAP);
	/* controller requires we set queue entry sizes and host page size. */
	uint32_t cmd = NVME_CC_MPS(4096 /*TODO: arch-dep*/)
	               | NVME_CC_IOCQES(LOG2(sizeof(struct nvme_cmp)))
	               | NVME_CC_IOSQES(LOG2(sizeof(struct nvme_cmd)));

	/* try for weighted-round-robin */
	if(caps & (1 << 17))  // wrr w/ urgent
		cmd |= (1 << 11); // enable arb method

	/* controller requires we set the AQA register before enabling. */
	uint32_t aqa = NVME_ASQS | (NVME_ACQS << 16);
	nvme_reg_write32(nc, NVME_REG_AQA, aqa);

	/* allocate the admin queue */
	objid_t aq_id;
	int r = twz_object_create(TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE, 0, 0, &aq_id);
	if(r)
		return r;

	r = sys_opin(aq_id, &nc->aq_pin, 0);
	if(r)
		return r;

	size_t q_total_len = NVME_ASQS * sizeof(struct nvme_cmd) + NVME_ASQS * sizeof(struct nvme_cmp);

	r = sys_octl(aq_id, OCO_CACHE_MODE, 0x1000, q_total_len, OC_CM_UC);
	if(r)
		return r;
	twz_object_open(&nc->qo, aq_id, FE_READ | FE_WRITE);

	objid_t cid = twz_object_id(&nc->co);
	r = sys_octl(aq_id, OCO_MAP, 0x1000, q_total_len, (long)&cid);
	if(r)
		return r;

	/*
	struct sys_kaction_args args = {
	    .id = twz_object_id(&nc->co),
	    .cmd = KACTION_CMD_DEVICE_ENABLE_IOMMU,
	    .arg = 0,
	    .flags = KACTION_VALID,
	};
	if((r = sys_kaction(1, &args)) < 0) {
	    fprintf(stderr, "kaction: %d\n", r);
	    return r;
	}
	if(args.result) {
	    fprintf(stderr, "kaction-result: %d\n", args.result);
	    return r;
	}
	*/
	twz_object_open(&nc->qo, aq_id, FE_READ | FE_WRITE);

	fprintf(stderr, "[nvme] allocated aq @ %lx\n", nc->aq_pin);
	struct nvme_queue *queues =
	  calloc(1024 /* TODO: read max nr queues */, sizeof(struct nvme_queue));
	nc->queues = queues;
	nc->nr_queues = 1;

	nvmeq_init(&queues[0],
	  (void *)((uint64_t)twz_obj_base(&nc->qo)),
	  (void *)((uint64_t)twz_obj_base(&nc->qo) + NVME_ASQS * sizeof(struct nvme_cmd)),
	  NVME_ASQS,
	  nvmec_get_doorbell(nc, 0, false),
	  nvmec_get_doorbell(nc, 0, true));
	nvme_reg_write64(nc, NVME_REG_ASQ, nc->aq_pin + OBJ_NULLPAGE_SIZE);
	nvme_reg_write64(
	  nc, NVME_REG_ACQ, nc->aq_pin + OBJ_NULLPAGE_SIZE + NVME_ASQS * sizeof(struct nvme_cmd));

	if(!nc->msix) {
		/* disable all interrupts for now */
		nvme_reg_write32(nc, NVME_REG_INTMS, 0xFFFFFFFF);
	}

	/* set the command, and set the EN bit in a separate write. Note: this might not be necessary,
	 * but this isn't performance critical and the spec doesn't make it clear if these can be
	 * combined. This is safer. :) */
	nvme_reg_write32(nc, NVME_REG_CC, cmd);
	nvme_reg_write32(nc, NVME_REG_CC, cmd | NVME_CC_EN);

	nc->dstride = 1 << (NVME_CAP_DSTRD(caps) + 2);
	fprintf(stderr, "[nvme] dstride is %d\n", nc->dstride);
	if(!nc->msix) {
		/* unmask all interrupts */
		nvme_reg_write32(nc, NVME_REG_INTMC, 0xFFFFFFFF);
	}
	return 0;
}

int nvmec_check_features(struct nvme_controller *nc)
{
	struct pcie_function_header *hdr = twz_device_getds(&nc->co);
	if(hdr->classid != 1 || hdr->subclassid != 8 || hdr->progif != 2) {
		fprintf(stderr,
		  "[nvme]: controller is not an NVMe controller (%x %x %x)\n",
		  hdr->classid,
		  hdr->subclassid,
		  hdr->progif);
		return -1;
	}

	uint64_t caps = nvme_reg_read64(nc, NVME_REG_CAP);
	uint32_t vs = nvme_reg_read32(nc, NVME_REG_VS);
	if(!(caps & (1ul << 37))) {
		fprintf(stderr, "[nvme] controller does not support NVM command set\n");
		return -1;
	}
	if(vs >> 16 == 0) {
		fprintf(stderr, "[nvme] controller reports bad version number (%x)\n", vs);
		return -1;
	}
	return 0;
}

void nvme_cmp_decode(uint64_t sp, uint32_t *dw0, uint32_t *dw3)
{
	*dw0 = sp >> 32;
	*dw3 = sp & 0xffffffff;
}

#define NVME_CMP_DW3_STATUS(x) ((x) >> 17)

#include <limits.h>

int nvmec_execute_cmd(struct nvme_controller *nc,
  struct nvme_cmd *cmd,
  uint16_t *sr,
  uint32_t *cres)
{
	uint16_t cid = nvmeq_submit_cmd(&nc->queues[0], cmd);
	struct sys_thread_sync_args sa[2] = {
		[0] = {
		.addr = (uint64_t *)&nc->queues[0].sps[cid],
		.op = THREAD_SYNC_SLEEP,
		}, [1] = {
			.addr = (uint64_t *)&nc->sp_error,
			.op = THREAD_SYNC_SLEEP,
		},
	};
	uint64_t res, err;
	while(1) {
		res = nc->queues[0].sps[cid];
		err = atomic_exchange(&nc->sp_error, 0);
		if(res || err)
			break;
		int r = sys_thread_sync(2, sa);
		if(r < 0) {
			return r;
		}
	}
	if(err == 1) {
		return -1;
	}
	uint32_t cr, _sr;
	nvme_cmp_decode(res, &cr, &_sr);
	uint16_t status = NVME_CMP_DW3_STATUS(_sr);
	*sr = status;
	*cres = cr;
	return 0;
}
int nvmec_identify(struct nvme_controller *nc)
{
	struct nvme_cmd cmd;
	void *ci_memory = (void *)((uintptr_t)twz_obj_base(&nc->qo) + 0x200000);
	void *nsl_memory = (void *)((uintptr_t)twz_obj_base(&nc->qo) + 0x200000 + 0x1000);
	void *ns_memory = (void *)((uintptr_t)twz_obj_base(&nc->qo) + 0x200000 + 0x2000);

	objid_t id = twz_object_id(&nc->co);
	int r =
	  sys_octl(twz_object_id(&nc->qo), OCO_MAP, OBJ_NULLPAGE_SIZE + 0x200000, 0x3000, (long)&id);
	if(r)
		return r;

	nvme_cmd_init_identify(&cmd, 1, 0, nc->aq_pin + 0x200000 + 0x1000);
	uint32_t cres;
	uint16_t status;
	if(nvmec_execute_cmd(nc, &cmd, &status, &cres))
		return -1;
	if(status) {
		fprintf(stderr, "[nvme] identify failed (command error %x)\n", status);
		return -1;
	}

	struct nvme_controller_ident *ci = ci_memory;
	(void)ci; // TODO: check features

	struct nvme_namespace_ident *nsi = ns_memory;
	uint32_t *nsl = nsl_memory;

	nvme_cmd_init_identify(&cmd, 2, 0, nc->aq_pin + 0x200000 + 0x2000);
	if(nvmec_execute_cmd(nc, &cmd, &status, &cres))
		return -1;
	if(status) {
		fprintf(stderr, "[nvme] identify namespace IDs failed (command error %x)\n", status);
		return -1;
	}

	for(uint32_t n = 0; n < 1024; n++) {
		if(!nsl[n])
			continue;
		nvme_cmd_init_identify(&cmd, 0, nsl[n], nc->aq_pin + 0x200000 + 0x3000);
		if(nvmec_execute_cmd(nc, &cmd, &status, &cres))
			return -1;
		if(status) {
			fprintf(stderr, "[nvme] identify namespace failed (command error %x)\n", status);
			return -1;
		}
		uint32_t lbaf = nsi->lbaf[nsi->flbas & 15];
		if(lbaf & 0xffff) {
			fprintf(stderr, "[nvme] namespace supports metadata, which I dont :)\n");
			continue;
		}
		struct nvme_namespace *ns = malloc(sizeof(*ns));
		uint32_t lba_size = (1 << ((lbaf >> 16) & 0xff));
		ns->size = nsi->nsze * lba_size;
		ns->use = nsi->nuse * lba_size;
		ns->cap = nsi->ncap * lba_size;
		ns->lba_size = lba_size;
		ns->id = nsl[n];
		ns->next = nc->namespaces;
		nc->namespaces = ns;
		fprintf(stderr,
		  "[nvme] identified namespace (sz=%ld bytes, cap=%ld bytes, lba=%d bytes)\n",
		  ns->size,
		  ns->cap,
		  ns->lba_size);
	}

	return 0;
}

struct nvme_cmp *nvmeq_cq_peek(struct nvme_queue *q, uint32_t i, bool *phase)
{
	uint32_t index = (q->cmpq.head + i) & (q->count - 1);
	debug_printf("peek %d (%d %d) %p\n", index, q->cmpq.head, i, q->cmpq.entries);
	struct nvme_cmp *c = (struct nvme_cmp *)&q->cmpq.entries[index];
	*phase = q->cmpq.phase ^ (index < q->cmpq.head);
	return c;
}

void nvmeq_sq_adv_head(struct nvme_queue *q, uint32_t head)
{
	bool wr = head < q->subq.head;
	q->subq.head = head;
	q->subq.phase ^= wr;
}

void nvmeq_cq_consume(struct nvme_queue *q, uint32_t c)
{
	uint32_t head = (q->cmpq.head + c) & (q->count - 1);
	bool wr = head < q->cmpq.head;
	q->cmpq.head = head;
	q->cmpq.phase ^= wr;
}

#define NVME_CMP_DW2_HEAD(x) ((x)&0xffff)
#define NVME_CMP_DW3_PHASE (1 << 16)

void nvmeq_interrupt(struct nvme_controller *nc, struct nvme_queue *q)
{
	/* try to advance the queue. Note that, since cid is never zero, any all-zero entries can be
	 * totally skipped, regardless of phase */
	uint32_t i = 0;
	bool more = true;
	struct nvme_cmp *buf[16];
	while(i < 16) {
		bool phase;
		struct nvme_cmp *cmp = nvmeq_cq_peek(q, i, &phase);
		// debug_printf("peeked at %x (%d %d) -> %d\n",
		// cmp->cmp_dword[3],
		//!!(cmp->cmp_dword[3] & NVME_CMP_DW3_PHASE),
		// phase,
		// cmp->cmp_dword[3] & 0xffff);
		if(!!(cmp->cmp_dword[3] & NVME_CMP_DW3_PHASE) != phase) {
			more = false;
			break;
		}
		uint32_t head = NVME_CMP_DW2_HEAD(cmp->cmp_dword[2]);
		nvmeq_sq_adv_head(q, head);
		buf[i] = cmp;
		i++;
	}
	if(i > 0) {
		nvmeq_cq_consume(q, i);
	}
	struct sys_thread_sync_args sas[16];
	for(uint32_t j = 0; j < i; j++) {
		// debug_printf(":: :: :: %x %x\n", buf[j]->cmp_dword[3], buf[j]->cmp_dword[2]);
		uint16_t cid = buf[j]->cmp_dword[3] & 0xffff;
		uint64_t result = ((uint64_t)buf[j]->cmp_dword[0] << 32) | buf[j]->cmp_dword[3];
		q->sps[cid] = result;
		sas[j] = (struct sys_thread_sync_args){
			.addr = (uint64_t *)&q->sps[cid],
			.arg = INT_MAX,
			.op = THREAD_SYNC_WAKE,
		};
	}
	sys_thread_sync(i, sas);
	if(more)
		nvmeq_interrupt(nc, q);
}

void nvme_wait_for_event(struct nvme_controller *nc)
{
	struct device_repr *repr = twz_device_getrepr(&nc->co);
	struct sys_thread_sync_args sa[2] = { [0] = { .addr = &repr->syncs[DEVICE_SYNC_IOV_FAULT],
		                                    .op = THREAD_SYNC_SLEEP },
		[1] = { .addr = &repr->interrupts[0].sp, .op = THREAD_SYNC_SLEEP

		} };
	for(;;) {
		debug_printf("NVME WAIT\n");
		uint64_t iovf = atomic_exchange(&repr->syncs[DEVICE_SYNC_IOV_FAULT], 0);
		uint64_t irq = atomic_exchange(&repr->interrupts[0].sp, 0);
		debug_printf(":: %lx %lx\n", iovf, irq);
		if(iovf & 1) {
			/* handled; retry */
			for(size_t i = 0; i < nc->nr_queues; i++) {
				nvmeq_interrupt(nc, &nc->queues[i]);
			}
			nc->sp_error = 1;
			struct sys_thread_sync_args sa_er = {
				.addr = (uint64_t *)&nc->sp_error,
				.op = THREAD_SYNC_WAKE,
				.arg = INT_MAX,
			};
			debug_printf("Notifying wakeup on error\n");
			int r = sys_thread_sync(1, &sa_er);
			if(r) {
				fprintf(stderr, "[nvme] thread_sync error: %d\n", r);
				return;
			}
		} else if(iovf) {
			fprintf(stderr, "[nvme] unhandled IOMMU error!\n");
			exit(1);
		}
		if(irq) {
			for(size_t i = 0; i < nc->nr_queues; i++) {
				nvmeq_interrupt(nc, &nc->queues[i]);
			}
			volatile struct nvme_cmp *cmp = nc->queues[0].cmpq.entries;
			for(int i = 0; i < 10; i++) {
				debug_printf("CMP %x %x %x %x\n",
				  cmp[i].cmp_dword[0],
				  cmp[i].cmp_dword[1],
				  cmp[i].cmp_dword[2],
				  cmp[i].cmp_dword[3]);
			}
		}
		if(!iovf && !irq) {
			int r = sys_thread_sync(2, sa);
			if(r < 0) {
				fprintf(stderr, "[nvme] thread_sync error: %d\n", r);
				return;
			}
		}
	}
}

#include <pthread.h>

void *ptm(void *arg)
{
	nvmec_identify(arg);

	return 0;
}

int main(int argc, char **argv)
{
	if(!argv[1] || argc == 1) {
		fprintf(stderr, "usage: nvme controller-name\n");
		return 1;
	}

	struct nvme_controller nc = {};
	int r = twz_object_open_name(&nc.co, argv[1], FE_READ | FE_WRITE);
	if(r) {
		fprintf(stderr, "nvme: failed to open controller %s: %d\n", argv[1], r);
		return 1;
	}

	printf("[nvme] starting NVMe controller %s\n", argv[1]);

	if(nvmec_check_features(&nc))
		return 1;

	if(nvmec_reset(&nc))
		return -1;

	if(nvmec_pcie_init(&nc))
		return -1;

	if(nvmec_init(&nc))
		return -1;

	if(nvmec_wait_for_ready(&nc))
		return -1;
	nc.init = true;

	pthread_t pt;
	pthread_create(&pt, NULL, ptm, &nc);
	nvme_wait_for_event(&nc);
	for(volatile long i = 0; i < 1000000000; i++) {
	}
	debug_printf("TRYING IDENT AGAIN\n");
	nvmec_identify(&nc);
	for(volatile long i = 0; i < 1000000000; i++) {
	}
	debug_printf("TRYING IDENT AGAIN\n");
	nvmec_identify(&nc);

	printf("[nvme] successfully started controller\n");
	return 0;
}
