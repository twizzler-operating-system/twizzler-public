#include <init.h>
#include <interrupt.h>
#include <processor.h>
#include <syscall.h>

#include <limits.h>

#include <driver/device.h>

static struct object *kbd_obj;

static void __kbd_interrupt(int v, struct interrupt_handler *ih)
{
	(void)v;
	(void)ih;
	long tmp = 1;
	obj_write_data(kbd_obj, offsetof(struct device_repr, syncs[0]), sizeof(long), &tmp);
	thread_wake_object(
	  kbd_obj, offsetof(struct device_repr, syncs[0]) + OBJ_NULLPAGE_SIZE, INT_MAX);
}

static struct interrupt_handler _kbd_ih = {
	.fn = __kbd_interrupt,
};

static void __late_init_kbd(void *a __unused)
{
	int r;
	objid_t id;
	/* TODO: restrict write access. In fact, do this for ALL KSOs. */
	r = syscall_ocreate(0, 0, 0, 0, MIP_DFL_READ | MIP_DFL_WRITE, &id);
	if(r < 0)
		panic("failed to create keyboard object: %d", r);

	kbd_obj = obj_lookup(id);

	assert(kbd_obj != NULL);

	struct device_repr repr = {
		.hdr.name = "PS/2 Keyboard",
		.device_type = DEVICE_INPUT,
		.device_id = DEVICE_ID_KEYBOARD,
	};

	obj_write_data(kbd_obj, 0, sizeof(repr), &repr);
	kso_root_attach(kbd_obj, 0, KSO_DEVICE);

	interrupt_register_handler(33, &_kbd_ih);
}
POST_INIT(__late_init_kbd, NULL);
