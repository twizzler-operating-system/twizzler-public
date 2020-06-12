#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <twz/obj.h>

#include <twz/__cpp_compat.h>

#ifdef __cplusplus
#include <atomic>
using std::atomic_uint_least32_t;
using std::atomic_uint_least64_t;
#else /* not __cplusplus */
#include <stdatomic.h>
#endif /* __cplusplus */

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
#define NVME_REG_SQnTDBL(n, s) (0x1000 + (2 * (n) * (s)))
#define NVME_REG_CQnHDBL(n, s) (0x1000 + ((2 * (n) + 1) * (s)))

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

struct nvme_controller_ident {
	uint16_t vendor;
	uint16_t sub_vendor;
	uint8_t serial[20];
	uint8_t model[40];
	uint64_t fr;
	uint8_t rab;
	uint8_t ieee[3];
	uint8_t cmic;
	uint8_t mdts;
	uint16_t cntlid;
	uint32_t ver;
	uint32_t rtd3r;
	uint32_t rtd3e;
	uint32_t oaes;
	uint32_t ctrarr;
	uint8_t resv[12];
	__int128 fguid;
	uint8_t resv1[128];
	uint8_t resv2[256];
	uint8_t sqes;
	uint8_t cqes;
	uint16_t maxcmd;
	uint32_t nn;
	uint16_t oncs;
	uint8_t ign[46];
	uint8_t subnqn[256];
} __attribute__((packed));

struct nvme_namespace_ident {
	uint64_t nsze;
	uint64_t ncap;
	uint64_t nuse;
	uint8_t nsfeat;
	uint8_t nlbaf;
	uint8_t flbas;
	uint8_t mc;
	uint8_t ign[76];
	__int128 nguid;
	uint64_t eui64;
	uint32_t lbaf[16];
} __attribute__((packed));

static_assert(offsetof(struct nvme_controller_ident, sqes) == 512, "");
static_assert(offsetof(struct nvme_namespace_ident, lbaf) == 128, "");

// Scatter gather list
struct nvme_sgl {
	char data[16];
};

static_assert(sizeof(struct nvme_sgl) == 16, "");

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

static_assert(sizeof(struct nvme_cmd_hdr) == 40, "");

struct nvme_cmd {
	struct nvme_cmd_hdr hdr;
	// cmd dwords 10 thru 15
	uint32_t cmd_dword_10[6];
};

enum nvme_admin_op {
	NVME_ADMIN_OP_IDENTIFY = 0x6,
	NVME_ADMIN_SET_FEATURES = 0x9,
	NVME_ADMIN_CREATE_CQ = 0x5,
	NVME_ADMIN_CREATE_SQ = 0x1,
};

enum nvme_nvm_op {
	NVME_NVM_CMD_WRITE = 0x1,
	NVME_NVM_CMD_READ = 0x2,
};

enum nvme_feature_ident {
	NVME_CMD_SET_FEATURES_NQ = 0x7,
};

enum nvme_queue_priority {
	NVME_PRIORITY_URGENT = 0,
	NVME_PRIORITY_HIGH = 1,
	NVME_PRIORITY_MEDIUM = 2,
	NVME_PRIORITY_LOW = 3,
};

#define NVME_CMD_SDW0_CID(x) ((x) << 16)
#define NVME_CMD_SDW0_PSDT(x) ((x) << 14)
#define NVME_CMD_SDW0_FUSE(x) ((x) << 8)
#define NVME_CMD_SDW0_OP(x) ((x))

#define NVME_CMD_CREATE_CQ_CDW11_INT_EN (1 << 1)
#define NVME_CMD_CREATE_CQ_CDW11_PHYS_CONT (1 << 0)

static_assert(sizeof(struct nvme_cmd) == 64, "");

struct nvme_cmp {
	// Command specific
	uint32_t cmp_dword[4];
};

static_assert(sizeof(struct nvme_cmp) == 16, "");

struct nvme_namespace {
	uint64_t size;
	uint64_t cap;
	uint64_t use;
	uint32_t id;
	uint32_t lba_size;
	struct nvme_namespace *next;
	struct nvme_controller *nc;
};

struct nvme_queue {
	struct {
		volatile uint32_t *tail_doorbell;
		uint32_t head, tail;
		volatile struct nvme_cmd *entries;
		bool phase;
	} subq;
	struct {
		volatile uint32_t *head_doorbell;
		uint32_t head, tail;
		volatile struct nvme_cmp *entries;
		bool phase;
	} cmpq;
	atomic_uint_least64_t *sps;
	uint32_t count;
};

struct nvme_controller {
	twzobj co;
	twzobj qo;
	int dstride;
	bool init, msix;
	uint64_t aq_pin;
	int nrvec;
	size_t page_size;
	size_t max_queue_slots;
	atomic_uint_least64_t sp_error;
	struct nvme_queue admin_queue;
	struct nvme_queue *queues;
	size_t nr_queues;
	struct nvme_namespace *namespaces;
	twzobj ext_qobj;
};
