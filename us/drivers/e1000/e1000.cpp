#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <twz/name.h>
#include <twz/obj.h>
#include <twz/objctl.h>
#include <twz/queue.h>
#include <twz/sys.h>
#include <twz/thread.h>

#include <twz/debug.h>
#include <twz/driver/device.h>
#include <twz/driver/pcie.h>

#include "e1000.h"

#include <unistd.h>

#include <thread>

#define LOG2(X) ((unsigned)(8 * sizeof(unsigned long long) - __builtin_clzll((X)) - 1))
#define MIN(a, b) ({ (a) < (b) ? (a) : (b); })

#define BAR_MEMORY 0
#define BAR_FLASH 1
#define BAR_MSIX 3

void *e1000_co_get_regs(twzobj *co, int bar)
{
	struct pcie_function_header *hdr = (struct pcie_function_header *)twz_device_getds(co);
	return twz_object_lea(co, (void *)hdr->bars[bar]);
}

uint8_t e1000_reg_read8(e1000_controller *nc, int r, int bar)
{
	void *regs = e1000_co_get_regs(&nc->ctrl_obj, bar);
	return *(volatile uint8_t *)((char *)regs + r);
}

uint32_t e1000_reg_read32(e1000_controller *nc, int r, int bar)
{
	void *regs = e1000_co_get_regs(&nc->ctrl_obj, bar);
	return *(volatile uint32_t *)((char *)regs + r);
}

uint64_t e1000_reg_read64(e1000_controller *nc, int r, int bar)
{
	void *regs = e1000_co_get_regs(&nc->ctrl_obj, bar);
	return *(volatile uint64_t *)((char *)regs + r);
}

void e1000_reg_write32(e1000_controller *nc, int r, int bar, uint32_t v)
{
	void *regs = e1000_co_get_regs(&nc->ctrl_obj, bar);
	*(volatile uint32_t *)((char *)regs + r) = v;
	asm volatile("sfence;" ::: "memory");
}

void e1000_reg_write64(e1000_controller *nc, int r, int bar, uint64_t v)
{
	void *regs = e1000_co_get_regs(&nc->ctrl_obj, bar);
	*(volatile uint64_t *)((char *)regs + r) = v;
	asm volatile("sfence;" ::: "memory");
}

int e1000c_reset(e1000_controller *nc)
{
	e1000_reg_write32(nc, REG_RCTRL, BAR_MEMORY, 0);
	e1000_reg_write32(nc, REG_TCTRL, BAR_MEMORY, 0);
	e1000_reg_write32(nc, REG_CTRL, BAR_MEMORY, CTRL_RST);
	usleep(1);
	while(e1000_reg_read32(nc, REG_CTRL, BAR_MEMORY) & CTRL_RST)
		asm volatile("pause");
	return 0;
}

#include <twz/driver/msi.h>
int e1000c_pcie_init(e1000_controller *nc)
{
	struct pcie_function_header *hdr =
	  (struct pcie_function_header *)twz_device_getds(&nc->ctrl_obj);
	struct pcie_config_space *space = twz_object_lea(&nc->ctrl_obj, hdr->space);
	/* bus-master enable, memory space enable. We can do interrupt disable too, since we'll be using
	 * MSI */
	space->header.command =
	  COMMAND_MEMORYSPACE | COMMAND_BUSMASTER | COMMAND_INTDISABLE | COMMAND_SERRENABLE;
	/* allocate an interrupt vector */
	int r;
	if((r = twz_object_kaction(&nc->ctrl_obj, KACTION_CMD_DEVICE_SETUP_INTERRUPTS, 1)) < 0) {
		fprintf(stderr, "kaction: %d\n", r);
		return -EINVAL;
	}

	/* try to use MSI-X, but fall back to MSI if not available */
	union pcie_capability_ptr cp;
	if(!pcief_capability_get(hdr, PCIE_MSIX_CAPABILITY_ID, &cp)) {
		fprintf(stderr, "[e1000] no interrupt generation method supported\n");
		return -ENOTSUP;
	}

	msix_configure(&nc->ctrl_obj, cp.msix, 1);
	return 0;
}

int e1000c_init(e1000_controller *nc)
{
	uint32_t rah = e1000_reg_read32(nc, REG_RAH, BAR_MEMORY);
	uint32_t ral = e1000_reg_read32(nc, REG_RAL, BAR_MEMORY);
	nc->mac[0] = ral & 0xff;
	nc->mac[1] = (ral >> 8) & 0xff;
	nc->mac[2] = (ral >> 16) & 0xff;
	nc->mac[3] = (ral >> 24) & 0xff;
	nc->mac[4] = rah & 0xff;
	nc->mac[5] = (rah >> 8) & 0xff;
	e1000_reg_write32(nc, REG_CTRL, BAR_MEMORY, CTRL_FD | CTRL_ASDE);

	if(twz_object_new(&nc->buf_obj, NULL, NULL, TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE))
		return -1;

	int r;
	r = twz_object_pin(&nc->buf_obj, &nc->buf_pin, 0);
	if(r)
		return r;
	r = twz_object_ctl(&nc->buf_obj, OCO_CACHE_MODE, 0, 0x8000, OC_CM_UC);
	if(r)
		return r;
	r = twz_device_map_object(&nc->ctrl_obj, &nc->buf_obj, 0, 0x1000000);
	if(r)
		return r;

	nc->nr_tx_desc = 0x1000 / sizeof(e1000_tx_desc);
	nc->nr_rx_desc = 0x1000 / sizeof(e1000_rx_desc);

	e1000_reg_write32(nc, REG_TXDESCLO, BAR_MEMORY, (uint32_t)(nc->buf_pin));
	e1000_reg_write32(nc, REG_TXDESCHI, BAR_MEMORY, (uint32_t)(nc->buf_pin >> 32));
	e1000_reg_write32(nc, REG_RXDESCLO, BAR_MEMORY, (uint32_t)((nc->buf_pin + 0x1000)));
	e1000_reg_write32(nc, REG_RXDESCHI, BAR_MEMORY, (uint32_t)((nc->buf_pin + 0x1000) >> 32));

	nc->tx_ring = (struct e1000_tx_desc *)twz_object_base(&nc->buf_obj);
	nc->rx_ring = (struct e1000_rx_desc *)((char *)nc->tx_ring + 0x1000);

	for(size_t i = 0; i < nc->nr_rx_desc; i++) {
		nc->rx_ring[i].status = 0;
		nc->rx_ring[i].addr = nc->buf_pin + 0x8000 + 0x1000 * i;
		nc->rx_ring[i].length = 0x1000;
	}

	for(size_t i = 0; i < nc->nr_tx_desc; i++) {
		nc->tx_ring[i].status = 0;
		nc->tx_ring[i].cmd = 0;
	}

	e1000_reg_write32(nc, REG_TXDESCLEN, BAR_MEMORY, 0x1000);
	e1000_reg_write32(nc, REG_RXDESCLEN, BAR_MEMORY, 0x1000);

	e1000_reg_write32(nc, REG_TXDESCHEAD, BAR_MEMORY, 0);
	e1000_reg_write32(nc, REG_TXDESCTAIL, BAR_MEMORY, 0);
	e1000_reg_write32(nc, REG_RXDESCHEAD, BAR_MEMORY, 0);
	e1000_reg_write32(nc, REG_RXDESCTAIL, BAR_MEMORY, nc->nr_rx_desc - 1);

	e1000_reg_write32(nc,
	  REG_IVAR,
	  BAR_MEMORY,
	  IVAR_EN_RxQ0 | IVAR_EN_RxQ1 | IVAR_EN_TxQ0 | IVAR_EN_TxQ1 | IVAR_EN_OTHER);
	e1000_reg_write32(nc, REG_IAM, BAR_MEMORY, 0);
	e1000_reg_write32(nc, REG_IMC, BAR_MEMORY, 0xffffffff);
	e1000_reg_write32(
	  nc, REG_IMS, BAR_MEMORY, ICR_LSC | ICR_RXO | ICR_RxQ0 | ICR_RxQ1 | ICR_TxQ0 | ICR_TxQ1);

	e1000_reg_write32(nc,
	  REG_RCTRL,
	  BAR_MEMORY,
	  RCTL_EN | RCTL_SBP | RCTL_UPE | RCTL_MPE | RCTL_LBM_NONE | RCTL_BAM | RCTL_SECRC
	    | RCTL_BSIZE_2048);

	e1000_reg_write32(nc,
	  REG_TCTRL,
	  BAR_MEMORY,
	  TCTL_EN | TCTL_PSP | (15 << TCTL_CT_SHIFT) | (64 << TCTL_COLD_SHIFT) | TCTL_RTLC);

	e1000_reg_write32(nc, REG_CTRL, BAR_MEMORY, CTRL_FD | CTRL_ASDE | CTRL_SLU);
	e1000_reg_write32(nc, REG_CTRL_EXT, BAR_MEMORY, ECTRL_DRV_LOAD);
	fprintf(stderr,
	  "[e1000] init controller for MAC %2.2x:%2.2x:%2.2x:%2.2x:%2.2x:%2.2x, %ld,%ld rx,tx descs\n",
	  nc->mac[0],
	  nc->mac[1],
	  nc->mac[2],
	  nc->mac[3],
	  nc->mac[4],
	  nc->mac[5],
	  nc->nr_rx_desc,
	  nc->nr_tx_desc);
	return 0;
}

void e1000_tx_desc_init(struct e1000_tx_desc *desc, uint64_t p, size_t len)
{
	desc->addr = p;
	desc->length = len;
	/* TODO: do we need to report status, etc? */
	desc->cmd = CMD_EOP | CMD_IFCS | CMD_RS | CMD_RPS;
	desc->status = 0;
}

int32_t e1000c_send_packet(e1000_controller *nc, uint64_t pdata, size_t len)
{
	if((nc->cur_tx + 1) % nc->nr_tx_desc == nc->head_tx) {
		return -EAGAIN;
	}

	uint32_t num = nc->cur_tx;
	struct e1000_tx_desc *desc = &nc->tx_ring[nc->cur_tx];
	e1000_tx_desc_init(desc, pdata, len);

	nc->cur_tx = (nc->cur_tx + 1) % nc->nr_tx_desc;
	e1000_reg_write32(nc, REG_TXDESCTAIL, BAR_MEMORY, nc->cur_tx);
	return num;
}

void e1000c_interrupt_recv(e1000_controller *nc, int q)
{
	if(q) {
		fprintf(stderr, "[e1000] got activity on RxQ1\n");
		return;
	}
	uint32_t head = e1000_reg_read32(nc, REG_RXDESCHEAD, BAR_MEMORY);

	struct queue_entry_packet packet;
	while(nc->head_rx != head) {
		struct e1000_rx_desc *desc = &nc->rx_ring[nc->head_rx];
		packet.objid = twz_object_guid(&nc->buf_obj);
		packet.pdata = desc->addr;
		packet.len = desc->length;
		packet.stat = 0;
		packet.qe.info = head;
		queue_submit(&nc->rxqueue_obj, (struct queue_entry *)&packet, 0);
		nc->head_rx = (nc->head_rx + 1) % nc->nr_rx_desc;
	}

	while(
	  queue_get_finished(&nc->rxqueue_obj, (struct queue_entry *)&packet, QUEUE_NONBLOCK) == 0) {
		//	fprintf(stderr, "got completion for %d\n", packet.qe.info);

		struct e1000_rx_desc *desc = &nc->rx_ring[nc->tail_rx];
		desc->status = 0;
		desc->addr = packet.pdata;
		e1000_reg_write32(nc, REG_RXDESCTAIL, BAR_MEMORY, nc->tail_rx);
		nc->tail_rx = (nc->tail_rx + 1) % nc->nr_rx_desc;
	}

	// e1000_reg_write32(nc, REG_RXDESCTAIL, nc->head_rx);

	// uint32_t tail = e1000_reg_read32(nc, REG_RXDESCTAIL, BAR_MEMORY);
	// fprintf(stderr, "got recv!!: %x %x\n", head, tail);
}

void e1000c_interrupt_send(e1000_controller *nc, int q)
{
	if(q) {
		fprintf(stderr, "[e1000] got activity on TxQ1\n");
		return;
	}
	uint32_t head = e1000_reg_read32(nc, REG_TXDESCHEAD, BAR_MEMORY);
	while(nc->head_tx != head) {
		struct e1000_tx_desc *desc = &nc->tx_ring[nc->head_tx];
		while(!(desc->status & STAT_DD)) {
			asm("pause");
		}

		tx_request *req = nullptr;
		{
			std::unique_lock<std::mutex> lck(nc->mtx);
			req = nc->txs[nc->head_tx];
			nc->txs[nc->head_tx] = nullptr;
		}

		if(req) {
			// fprintf(stderr, "found packet: %d -> %d\n", nc->head_tx, req->packet.qe.info);
			queue_complete(&nc->txqueue_obj, (struct queue_entry *)&req->packet, 0);
			delete req;
		}

		nc->head_tx = (nc->head_tx + 1) % nc->nr_tx_desc;
	}
}

void e1000c_interrupt(e1000_controller *nc)
{
	uint32_t icr = e1000_reg_read32(nc, REG_ICR, BAR_MEMORY);
	// fprintf(stderr, "ICR: %x\n", icr);
	e1000_reg_write32(nc, REG_ICR, BAR_MEMORY, icr);

	if(icr & ICR_LSC) {
		/* TODO */
		e1000_reg_write32(nc, REG_CTRL, BAR_MEMORY, CTRL_FD | CTRL_ASDE | CTRL_SLU);
	}
	if(icr & ICR_RXO) {
		fprintf(stderr, "[e1000] warning - recv queues overrun\n");
	}
	if(icr & ICR_RxQ0) {
		e1000c_interrupt_recv(nc, 0);
	}
	if(icr & ICR_RxQ1) {
		e1000c_interrupt_recv(nc, 1);
	}
	if(icr & ICR_TxQ0) {
		e1000c_interrupt_send(nc, 0);
	}
	if(icr & ICR_TxQ1) {
		e1000c_interrupt_send(nc, 1);
	}
}

void e1000_wait_for_event(e1000_controller *nc)
{
	kso_set_name(NULL, "e1000.event_handler");
	struct device_repr *repr = twz_device_getrepr(&nc->ctrl_obj);
	struct sys_thread_sync_args sa[MAX_DEVICE_INTERRUPTS + 1];
	twz_thread_sync_init(&sa[0], THREAD_SYNC_SLEEP, &repr->syncs[DEVICE_SYNC_IOV_FAULT], 0);
	twz_thread_sync_init(&sa[1], THREAD_SYNC_SLEEP, &repr->interrupts[0].sp, 0);
	for(;;) {
		uint64_t iovf = atomic_exchange(&repr->syncs[DEVICE_SYNC_IOV_FAULT], 0);
		if(iovf & 1) {
			fprintf(stderr, "[nvme] unhandled IOMMU error!\n");
			exit(1);
		}
		bool worked = false;
		uint64_t irq = atomic_exchange(&repr->interrupts[0].sp, 0);
		if(irq) {
			worked = true;
			e1000c_interrupt(nc);
		}
		if(!iovf && !worked) {
			int r = twz_thread_sync_multiple(2, sa, NULL);
			if(r < 0) {
				fprintf(stderr, "[nvme] thread_sync error: %d\n", r);
				return;
			}
		}
	}
}

#include <set>

void e1000_tx_queue(e1000_controller *nc)
{
	std::set<std::pair<objid_t, size_t>> mapped;
	while(1) {
		tx_request *req = new tx_request;
		queue_receive(&nc->txqueue_obj, (struct queue_entry *)&req->packet, 0);

		size_t offset = req->packet.pdata % OBJ_MAXSIZE;
		offset -= OBJ_NULLPAGE_SIZE;
		if(mapped.find(std::make_pair(req->packet.objid, offset)) == mapped.end()) {
			twzobj tmpobj;
			twz_object_init_guid(&tmpobj, req->packet.objid, FE_READ);
			int nr_prep = 128;
			twz_device_map_object(&nc->ctrl_obj, &tmpobj, offset, 0x1000 * nr_prep);
			for(int i = 0; i < nr_prep; i++) {
				mapped.insert(std::make_pair(req->packet.objid, offset + 0x1000 * i));
			}
		}

		{
			std::unique_lock<std::mutex> lck(nc->mtx);
			int32_t pn = e1000c_send_packet(nc, req->packet.pdata, req->packet.len);
			if(pn < 0) {
				fprintf(stderr, "TODO: dropped packet\n");
			} else {
				nc->txs.reserve(pn + 1);
				nc->txs[pn] = req;
			}
		}
	}
}

int main(int argc, char **argv)
{
	if(!argv[1] || argc < 4) {
		fprintf(stderr, "usage: e1000 controller-name tx-queue-name rx-queue-name\n");
		return 1;
	}

	e1000_controller nc = {};
	int r = twz_object_init_name(&nc.ctrl_obj, argv[1], FE_READ | FE_WRITE);
	if(r) {
		fprintf(stderr, "e1000: failed to open controller %s: %d\n", argv[1], r);
		return 1;
	}

	r = twz_object_init_name(&nc.txqueue_obj, argv[2], FE_READ | FE_WRITE);
	if(r) {
		fprintf(stderr, "e1000: failed to open txqueue\n");
		return 1;
	}

	r = twz_object_init_name(&nc.rxqueue_obj, argv[3], FE_READ | FE_WRITE);
	if(r) {
		fprintf(stderr, "e1000: failed to open rxqueue\n");
		return 1;
	}

	printf("[e1000] starting e1000 controller %s\n", argv[1]);

	if(e1000c_reset(&nc))
		return -1;

	if(e1000c_pcie_init(&nc))
		return -1;

	if(e1000c_init(&nc))
		return -1;

	nc.init = true;

	e1000c_interrupt(&nc);

	std::thread thr(e1000_tx_queue, &nc);

	e1000_wait_for_event(&nc);
	return 0;
}
