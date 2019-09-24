#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <twz/name.h>
#include <twz/obj.h>
#include <twz/sys.h>

#include <twz/debug.h>
#include <twz/driver/pcie.h>

#define NVME_REG_CAP 0
#define NVME_REG_VS 8
#define NVME_REG_INTMS 0xC
#define NVME_REG_INTMC 0x10
#define NVME_REG_CC 0x14
#define NVME_REG_CSTS 0x1c
#define NVME_REG_NSSR 0x20
#define NVME_REG_AQA 0x24
#define NVME_REG_ASQ 0x28
#define NVME_REG_ACQ 0x30
#define NVME_REG_SQnTDBL(n, s) (0x1000 + ((n) * (s)))
#define NVME_REG_CQnHDBL(n, s) (0x1000 + (((n) + 1) * (s)))

#define NVME_CAP_MPSMAX(c) (((c) >> 52) & 0xf)
#define NVME_CAP_MPSMIN(c) (((c) >> 48) & 0xf)
#define NVME_CAP_DSTRD(c) (((c) >> 32) & 0xf)
#define NVME_CAP_CQR (1 << 16)
#define NVME_CAP_MQES ((c)&0xff)

#define NVME_CC_IOCQES(c) ((c) << 20)
#define NVME_CC_IOSQES(c) ((c) << 16)
#define NVME_CC_MPS(c) ((LOG2(c) - 12) << 7)
#define NVME_CC_EN 1

#define NVME_CSTS_PP (1 << 5)
#define NVME_CSTS_CFS (1 << 1)
#define NVME_CSTS_RDY 1

// Scatter gather list
struct nvme_sgl {
	char data[16];
};

_Static_assert(sizeof(struct nvme_sgl) == 16, "");

// Physical region pointer
struct nvme_prp {
	uintptr_t addr;
};

union nvme_data_ptr {
	// Scatter gather list
	struct nvme_sgl sgl1;
	// Physical region page
	struct nvme_prp prpp[2];
};

// Command submission queue header common to all commands

struct nvme_cmd_hdr {
	// Command dword 0
	uint32_t cdw0;
	// namespace ID
	uint32_t nsid;
	uint64_t reserved1;
	// Metadata pointer
	uint64_t mptr;
	// Data pointer
	union nvme_data_ptr dptr;
};

_Static_assert(sizeof(struct nvme_cmd_hdr) == 40, "");

struct nvme_cmd {
	struct nvme_cmd_hdr hdr;
	// cmd dwords 10 thru 15
	uint32_t cmd_dword_10[6];
};

enum nvme_admin_op {
	NVME_ADMIN_OP_IDENTIFY = 0x6,
};

#define NVME_CMD_SDW0_CID(x) ((x) << 16)
#define NVME_CMD_SDW0_PSDT(x) ((x) << 14)
#define NVME_CMD_SDW0_FUSE(x) ((x) << 8)
#define NVME_CMD_SDW0_OP(x) ((x))

static void nvme_cmd_init_identify(struct nvme_cmd *cmd, uint8_t cns, uint8_t nsid, uint64_t addr)
{
	memset(cmd, 0, sizeof(*cmd));
	cmd->hdr.cdw0 = NVME_CMD_SDW0_OP(NVME_ADMIN_OP_IDENTIFY);
	cmd->hdr.nsid = nsid;
	cmd->hdr.dptr.prpp[0].addr = addr;
	cmd->cmd_dword_10[0] = cns;
}

_Static_assert(sizeof(struct nvme_cmd) == 64, "");

struct nvme_cmp {
	// Command specific
	uint32_t cmp_dword[4];
};

_Static_assert(sizeof(struct nvme_cmp) == 16, "");

struct nvme_controller {
	struct object co;
	struct object qo;
	int dstride;
	bool init, msix;
	uint64_t aq_pin;
	struct nvme_queue *queues;
};

struct nvme_queue {
	struct {
		volatile uint32_t *tail_doorbell;
		uint32_t head, tail;
		volatile struct nvme_cmd *entries;
	} subq;
	struct {
		volatile uint32_t *head_doorbell;
		uint32_t head, tail;
		volatile struct nvme_cmp *entries;
	} cmpq;
	uint64_t *sps;
	uint32_t count;
};

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
}

#include <stdatomic.h>
void nvmeq_submit_cmd(struct nvme_queue *q, struct nvme_cmd *cmd)
{
	size_t index = q->subq.tail;
	cmd->hdr.cdw0 |= (uint32_t)(index << 16);
	q->subq.entries[q->subq.tail] = *cmd;
	atomic_thread_fence(memory_order_acq_rel);
	q->subq.tail = (q->subq.tail + 1) & (q->count - 1);
	*q->subq.tail_doorbell = q->subq.tail;
}

void *nvme_co_get_regs(struct object *co)
{
	struct pcie_function_header *hdr = twz_obj_base(co);
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
	struct pcie_function_header *hdr = twz_obj_base(&nc->co);
	struct pcie_config_space *space = twz_ptr_lea(&nc->co, hdr->space);
	/* bus-master enable, memory space enable. We can do interrupt disable too, since we'll be using
	 * MSI */
	space->header.command =
	  COMMAND_MEMORYSPACE | COMMAND_BUSMASTER | COMMAND_INTDISABLE | COMMAND_SERRENABLE;
	/* allocate an interrupt vector */
	hdr->nr_interrupts = 1;
	hdr->interrupts[0].flags = PCIE_FUNCTION_INT_ENABLE;
	struct sys_kaction_args args = {
		.id = twz_object_id(&nc->co),
		.cmd = KACTION_CMD_PF_INTERRUPTS_SETUP,
		.arg = 0,
		.flags = KACTION_VALID,
	};
	int r;
	if((r = sys_kaction(1, &args)) < 0) {
		fprintf(stderr, "kaction: %d\n", r);
		return r;
	}
	if(args.result) {
		fprintf(stderr, "kaction-result: %d\n", args.result);
		return r;
	}
	fprintf(stderr, "[nvme] allocated vector %d for interrupts\n", hdr->interrupts[0].vec);

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
		tbl->data = device_msi_data(hdr->interrupts[0].vec, MSI_LEVEL);
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

	fprintf(stderr, "[nvme] allocated aq @ %lx\n", nc->aq_pin);
	struct nvme_queue *queues =
	  calloc(1024 /* TODO: read max nr queues */, sizeof(struct nvme_queue));
	nc->queues = queues;

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
	struct pcie_function_header *hdr = twz_obj_base(&nc->co);
	if(hdr->classid != 1 || hdr->subclassid != 8 || hdr->progif != 2) {
		fprintf(stderr, "[nvme]: controller is not an NVMe controller\n");
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

int nvmec_identify(struct nvme_controller *nc)
{
	struct nvme_cmd cmd;
	void *memory = (void *)((uintptr_t)twz_obj_base(&nc->qo) + 0x200000);
	nvme_cmd_init_identify(&cmd, 1, 0, nc->aq_pin + 0x200000 + 0x1000);
	nvmeq_submit_cmd(&nc->queues[0], &cmd);

	return 0;
}

void nvme_wait_for_event(struct nvme_controller *nc)
{
	struct pcie_function_header *hdr = twz_obj_base(&nc->co);
	struct sys_thread_sync_args sa[2] = { [0] = { .addr = &hdr->iov_fault,
		                                    .op = THREAD_SYNC_SLEEP },
		[1] = { .addr = &hdr->interrupts[0].sp, .op = THREAD_SYNC_SLEEP

		} };
	for(;;) {
		debug_printf("NVME WAIT\n");
		uint64_t iovf = atomic_exchange(&hdr->iov_fault, 0);
		uint64_t irq = atomic_exchange(&hdr->interrupts[0].sp, 0);
		debug_printf(":: %lx %lx\n", iovf, irq);
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

	for(;;)
		;
	return 0;
}

int main(int argc, char **argv)
{
	if(!argv[1]) {
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
