#include <stdio.h>
#include <twz/name.h>
#include <twz/obj.h>
#include <twz/sys.h>

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
#define NVME_REG_SQnTDBL(n, s) (0x1000 + ((n) * (4 << (s))))
#define NVME_REG_CQnHDBL(n, s) (0x1000 + (((n) + 1) * (4 << (s))))

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

_Static_assert(sizeof(struct nvme_cmd) == 64, "");

struct nvme_cmp {
	// Command specific
	uint32_t cmp_dword[4];
};

_Static_assert(sizeof(struct nvme_cmp) == 16, "");

struct nvme_controller {
	struct object co;
	int dstride;
};

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

int nvmec_pcie_init(struct nvme_controller *nc)
{
	struct pcie_function_header *hdr = twz_obj_base(&nc->co);
	struct pcie_config_space *space = twz_ptr_lea(&nc->co, hdr->space);
	/* bus-master enable, memory space enable. We can do interrupt disable too, since we'll be using
	 * MSI */
	space->header.command =
	  COMMAND_MEMORYSPACE | COMMAND_BUSMASTER | COMMAND_INTDISABLE | COMMAND_SERRENABLE;
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

#include <twz/debug.h>
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

	uint64_t aq_pin;
	r = sys_opin(aq_id, &aq_pin, 0);
	if(r)
		return r;

	size_t q_total_len = NVME_ASQS * sizeof(struct nvme_cmd) + NVME_ASQS * sizeof(struct nvme_cmp);

	r = sys_octl(aq_id, OCO_CACHE_MODE, 0x1000, q_total_len, OC_CM_UC);
	if(r)
		return r;

	fprintf(stderr, "[nvme] allocated aq @ %lx\n", aq_pin);
	nvme_reg_write64(nc, NVME_REG_ASQ, aq_pin + 0x1000);
	nvme_reg_write64(nc, NVME_REG_ACQ, aq_pin + 0x1000 + NVME_ASQS * sizeof(struct nvme_cmd));

	/* disable all interrupts for now */
	nvme_reg_write32(nc, NVME_REG_INTMS, 0xFFFFFFFF);

	/* set the command, and set the EN bit in a separate write. Note: this might not be necessary,
	 * but this isn't performance critical and the spec doesn't make it clear if these can be
	 * combined. This is safer. :) */
	nvme_reg_write32(nc, NVME_REG_CC, cmd);
	nvme_reg_write32(nc, NVME_REG_CC, cmd | NVME_CC_EN);

	nc->dstride = 1 << (NVME_CAP_DSTRD(caps) + 2);
	fprintf(stderr, "[nvme] dstride is %d\n", nc->dstride);
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

int main(int argc, char **argv)
{
	if(!argv[1]) {
		fprintf(stderr, "usage: nvme controller-name\n");
		return 1;
	}

	struct nvme_controller nc;
	int r = twz_object_open_name(&nc.co, argv[1], FE_READ | FE_WRITE);
	if(r) {
		fprintf(stderr, "nvme: failed to open controller %s: %d\n", argv[1], r);
		return 1;
	}

	printf("[nvme] starting NVMe controller %s\n", argv[1]);

	if(nvmec_check_features(&nc))
		return 1;

	if(nvmec_pcie_init(&nc))
		return -1;

	if(nvmec_reset(&nc))
		return -1;

	if(nvmec_init(&nc))
		return -1;

	if(nvmec_wait_for_ready(&nc))
		return -1;

	printf("[nvme] successfully started controller\n");
	return 0;
}
