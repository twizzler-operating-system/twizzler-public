#include <device.h>
#include <init.h>
#include <interrupt.h>
#include <machine/isa.h>
#include <object.h>
#include <processor.h>
#include <syscall.h>

#include <limits.h>

#include <twz/driver/device.h>

#include <arch/x86_64-io.h>

static struct object *kbd_obj;

static void __kbd_interrupt(int v, struct interrupt_handler *ih)
{
	(void)v;
	(void)ih;
	long tmp = x86_64_inb(0x60);
	static bool _f = false;
	if(tmp == 0xe1 && !_f) {
		processor_print_all_stats();
		thread_print_all_threads();
		mm_print_stats();
	}
	_f = !_f;
	device_signal_sync(kbd_obj, 0, tmp);
}

static struct interrupt_handler _kbd_ih = {
	.fn = __kbd_interrupt,
};

static void __late_init_kbd(void *a __unused)
{
	/* krc: move */
	kbd_obj = device_register(DEVICE_BT_ISA, DEVICE_ID_KEYBOARD);
	kso_setname(kbd_obj, "PS/2 Keyboard");

	kso_attach(pc_get_isa_bus(), kbd_obj, DEVICE_ID_KEYBOARD);

	interrupt_register_handler(33, &_kbd_ih);
}
POST_INIT(__late_init_kbd, NULL);
