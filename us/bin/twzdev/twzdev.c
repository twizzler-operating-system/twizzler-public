#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <twz/bstream.h>
#include <twz/driver/bus.h>
#include <twz/driver/device.h>
#include <twz/name.h>
#include <twz/obj.h>
#include <twz/pty.h>

void pcie_load_bus(twzobj *obj);

int create_pty_pair(char *server, char *client)
{
	twzobj pty_s, pty_c;

	int r;
	if((r = twz_object_new(&pty_s,
	      NULL,
	      NULL,
	      TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_VOLATILE | TWZ_OC_TIED_NONE))) {
		return r;
	}

	if((r = twz_object_new(&pty_c,
	      NULL,
	      NULL,
	      TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_VOLATILE | TWZ_OC_TIED_NONE))) {
		return r;
	}

	if((r = pty_obj_init_server(&pty_s, twz_object_base(&pty_s))))
		return r;

	struct pty_hdr *ps = twz_object_base(&pty_s);
	if((r = pty_obj_init_client(&pty_c, twz_object_base(&pty_c), ps)))
		return r;

	if((r = twz_name_assign(twz_object_guid(&pty_s), server))) {
		return r;
	}

	if((r = twz_name_assign(twz_object_guid(&pty_c), client))) {
		return r;
	}

	return 0;
}

int create_bstream(char *name)
{
	twzobj stream;
	int r;
	if((r = twz_object_new(&stream,
	      NULL,
	      NULL,
	      TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_VOLATILE | TWZ_OC_TIED_NONE))) {
		return r;
	}

	if((r = bstream_obj_init(&stream, twz_object_base(&stream), 16))) {
		return r;
	}

	return twz_name_assign(twz_object_guid(&stream), name);
}

struct isa_desc {
	uint64_t id;
	const char *drv;
	const char *desc;
	int pty;
};
struct isa_desc isa_devices[] = { { DEVICE_ID_KEYBOARD, "keyboard", "PS/2 Keyboard Driver", 0 },
	{ DEVICE_ID_SERIAL, "serial", "Serial Port Driver", 1 } };

static void start_stream_device(objid_t id)
{
	twzobj dobj;
	twz_object_init_guid(&dobj, id, FE_READ | FE_WRITE);
	struct device_repr *dr = twz_object_base(&dobj);

	struct isa_desc *desc = NULL;

	for(size_t i = 0; i < sizeof(isa_devices) / sizeof(isa_devices[0]) && desc == NULL; i++) {
		if(isa_devices[i].id == dr->device_id) {
			desc = &isa_devices[i];
		}
	}

	if(desc == NULL) {
		fprintf(stderr, "[twzdev]: unknown device ID %d\n", dr->device_id);
		return;
	}

	fprintf(stderr, "[twzdev] starting device driver: %s\n", desc->desc);

	char *target_name;
	if(desc->pty) {
		static int pty_ctr = 0;
		char *ps;
		char *pc;
		asprintf(&ps, "/dev/pty/ptyS%d", pty_ctr);
		asprintf(&pc, "/dev/pty/ptyS%dc", pty_ctr);
		create_pty_pair(ps, pc);
		pty_ctr++;
		target_name = ps;
	} else {
		char *s;
		asprintf(&s, "/dev/%s", desc->drv);
		target_name = s;
		create_bstream(s);
	}
	char *raw_name;
	asprintf(&raw_name, "/dev/raw/%s", desc->drv);
	twz_name_assign(twz_object_guid(&dobj), raw_name);

	char *exec;
	asprintf(&exec, "/usr/bin/%s", desc->drv);
	if(!fork()) {
		kso_set_name(NULL, "[driver] %s", desc->drv);
		execl(exec, exec, raw_name, target_name, NULL);
		exit(1);
	}
}

static void start_misc_device(objid_t id)
{
	twzobj dobj;
	twz_object_init_guid(&dobj, id, FE_READ | FE_WRITE);
	struct device_repr *dr = twz_object_base(&dobj);

	switch(dr->device_id) {
		case DEVICE_ID_FRAMEBUFFER:
			printf("[twzdev] registered framebuffer\n");
			create_pty_pair("/dev/pty/pty0", "/dev/pty/ptyc0");
			twz_name_assign(id, "/dev/framebuffer");
			break;
	}
}

int main()
{
	/* start devices */
	twzobj root;
	twz_object_init_guid(&root, 1, FE_READ);

	struct kso_root_repr *rr = twz_object_base(&root);
	for(size_t i = 0; i < rr->count; i++) {
		struct kso_attachment *k = &rr->attached[i];
		if(!k->id || !k->type)
			continue;
		switch(k->type) {
			twzobj dobj;
			case KSO_DEVBUS:
				twz_object_init_guid(&dobj, k->id, FE_READ | FE_WRITE);
				struct bus_repr *br = twz_bus_getrepr(&dobj);
				if(br->bus_type == DEVICE_BT_PCIE) {
					pcie_load_bus(&dobj);
				} else if(br->bus_type == DEVICE_BT_ISA) {
					/* TODO: REALLY NEED TO GENERIC THIS KSO CHILDREN STUFF */
					for(size_t i = 0; i < br->max_children; i++) {
						struct kso_attachment *k = twz_object_lea(&dobj, &br->children[i]);
						if(k->id == 0)
							continue;
						start_stream_device(k->id);
					}
				} else if(br->bus_type == DEVICE_BT_MISC) {
					for(size_t i = 0; i < br->max_children; i++) {
						struct kso_attachment *k = twz_object_lea(&dobj, &br->children[i]);
						if(k->id == 0)
							continue;
						start_misc_device(k->id);
					}
				} else if(br->bus_type == DEVICE_BT_SYSTEM) {
					/* nothing */
				} else {
					fprintf(stderr, "unknown bus_type: %d\n", br->bus_type);
				}

				break;
		}
	}

#if 1
	int r;
	if(!fork()) {
		kso_set_name(NULL, "[instance] nvme-driver");
		r = sys_detach(0, 0, TWZ_DETACH_ONENTRY | TWZ_DETACH_ONSYSCALL(SYS_BECOME), KSO_SECCTX);
		if(r) {
			fprintf(stderr, "failed to detach: %d\n", r);
		}

		execvp("nvme", (char *[]){ "nvme", "/dev/nvme", NULL });
		fprintf(stderr, "failed to start nvme driver\n");
		exit(1);
	}
#endif
}
