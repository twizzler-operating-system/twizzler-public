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

void start_stream_device(objid_t id)
{
	twzobj dobj;
	twz_object_init_guid(&dobj, id, FE_READ);

	int r;
	objid_t uid;
	if(twz_object_create(TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE | TWZ_OC_TIED_NONE, 0, 0, &uid)) {
		fprintf(stderr, "failed to create object for stream\n");
		return;
	}

	twzobj stream;
	twz_object_init_guid(&stream, uid, FE_READ | FE_WRITE);

	if((r = bstream_obj_init(&stream, twz_object_base(&stream), 16))) {
		fprintf(stderr, "failed to init bstream");
		abort();
	}

	struct device_repr *dr = twz_object_base(&dobj);
	fprintf(stderr, "[init] starting device driver: %d %s\n", dr->device_id, dr->hdr.name);
	int status;
	if(dr->device_id == DEVICE_ID_KEYBOARD) {
		twz_name_assign(uid, "dev:input:keyboard");
		if(!fork()) {
			kso_set_name(NULL, "[instance] keyboard");
			twz_name_assign(id, "dev:raw:keyboard");
			execv("/usr/bin/keyboard",
			  (char *[]){ "/usr/bin/keyboard", "dev:raw:keyboard", "dev:input:keyboard", NULL });
			exit(1);
		}
		// twz_name_assign(uid, "dev:input:keyboard");
	}
	if(dr->device_id == DEVICE_ID_SERIAL) {
		create_pty_pair("dev:pty:ptyS0", "dev:pty:ptyS0c");
		if(!fork()) {
			kso_set_name(NULL, "[instance] serial");
			twz_name_assign(id, "dev:raw:serial");
			execv("/usr/bin/serial",
			  (char *[]){ "/usr/bin/serial", "dev:raw:serial", "dev:pty:ptyS0", NULL });
			exit(1);
		}
	}
}

int main()
{
	printf("Hello from device manager\n");

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
			int pid, status;
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

				} else {
					fprintf(stderr, "unknown bus_type: %d\n", br->bus_type);
				}

				break;
		}
	}

	int r;
#if 1
	if(!fork()) {
		kso_set_name(NULL, "[instance] nvme-driver");
		r = sys_detach(0, 0, TWZ_DETACH_ONENTRY | TWZ_DETACH_ONSYSCALL(SYS_BECOME), KSO_SECCTX);
		if(r) {
			fprintf(stderr, "failed to detach: %d\n", r);
			twz_thread_exit(1);
		}

		execvp("nvme", (char *[]){ "nvme", "dev:controller:nvme", NULL });
		fprintf(stderr, "failed to start nvme driver\n");
		exit(1);
	}
#endif
}
