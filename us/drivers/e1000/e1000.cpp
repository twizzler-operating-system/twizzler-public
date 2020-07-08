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
	r = twz_device_map_object(&nc->ctrl_obj, &nc->buf_obj, 0, 0x8000);
	if(r)
		return r;

	nc->nr_tx_desc = 0x1000 / sizeof(e1000_tx_desc);
	nc->nr_rx_desc = 0x1000 / sizeof(e1000_rx_desc);
	e1000_reg_write32(nc, REG_TXDESCLO, BAR_MEMORY, (uint32_t)(nc->buf_pin));
	e1000_reg_write32(nc, REG_TXDESCHI, BAR_MEMORY, (uint32_t)(nc->buf_pin >> 32));
	e1000_reg_write32(nc, REG_RXDESCLO, BAR_MEMORY, (uint32_t)((nc->buf_pin + 0x1000)));
	e1000_reg_write32(nc, REG_RXDESCHI, BAR_MEMORY, (uint32_t)((nc->buf_pin + 0x1000) >> 32));

	e1000_reg_write32(nc, REG_TXDESCLEN, BAR_MEMORY, 0x1000);
	e1000_reg_write32(nc, REG_RXDESCLEN, BAR_MEMORY, 0x1000);

	e1000_reg_write32(nc, REG_TXDESCHEAD, BAR_MEMORY, 0);
	e1000_reg_write32(nc, REG_TXDESCTAIL, BAR_MEMORY, 0);
	e1000_reg_write32(nc, REG_RXDESCHEAD, BAR_MEMORY, 0);
	e1000_reg_write32(nc, REG_RXDESCTAIL, BAR_MEMORY, nc->nr_rx_desc - 1);

	e1000_reg_write32(nc, REG_IVAR, BAR_MEMORY, 0);

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
	return 0;
}

int main(int argc, char **argv)
{
	if(!argv[1] || argc < 2) {
		fprintf(stderr, "usage: e1000 controller-name queue-name\n");
		return 1;
	}

	e1000_controller nc = {};
	int r = twz_object_init_name(&nc.ctrl_obj, argv[1], FE_READ | FE_WRITE);
	if(r) {
		fprintf(stderr, "e1000: failed to open controller %s: %d\n", argv[1], r);
		return 1;
	}
	// r = twz_object_init_name(&nc.ext_qobj, argv[2], FE_READ | FE_WRITE);
	// if(r) {
	//		fprintf(stderr, "e1000: failed to open queue\n");
	//		return 1;
	//	}

	printf("[e1000] starting e1000 controller %s\n", argv[1]);

	if(e1000c_reset(&nc))
		return -1;

	if(e1000c_pcie_init(&nc))
		return -1;

	if(e1000c_init(&nc))
		return -1;

	// if(e1000c_wait_for_ready(&nc))
	//	return -1;
	nc.init = true;

	//	std::thread thr(ptm, &nc);

	//	e1000_wait_for_event(&nc);
	return 0;
}
