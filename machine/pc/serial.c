#include <arch/x86_64-io.h>
#include <processor.h>
#include <instrument.h>
#include <debug.h>
#include <interrupt.h>
#define PORT 0x3f8   /* COM1 */

void serial_init()
{
    x86_64_outb(PORT + 3, 0x80); // set up to load divisor latch
    x86_64_outb(PORT + 0, 3); // lsb
    x86_64_outb(PORT + 1, 0); // msb
    x86_64_outb(PORT + 3, 3); // 8N1
    x86_64_outb(PORT + 2, 0x07); // enable FIFO, clear, 14-byte threshold
	x86_64_outb(PORT + 1, 0x1);
}

static void _serial_interrupt(int i)
{
	printk("RESET\n");
	(void)i;
	arch_processor_reset();
}

static struct interrupt_handler _serial_handler = {
	.fn = _serial_interrupt,
};

__initializer static void _init_serial(void)
{
	int a = 0x24;
	interrupt_register_handler(a, &_serial_handler);
	arch_interrupt_unmask(a);
}

static int is_transmit_empty()
{
	return x86_64_inb(PORT + 5) & 0x20;
}

static int serial_getc(void)
{
	return x86_64_inb(PORT);
}

static void serial_putc(unsigned char byte)
{
	while (is_transmit_empty() == 0);
	x86_64_outb(PORT, byte);
}

int serial_received()
{
	return x86_64_inb(PORT + 5) & 1;
}


#include <spinlock.h>
static struct spinlock _lock = SPINLOCK_INIT;
__noinstrument
void debug_puts(char *s)
{
#if CONFIG_INSTRUMENT
	instrument_disable();
#endif

	bool fl = spinlock_acquire(&_lock);
	while(*s) {
		serial_putc(*s);
		if(*s == '\n')
			serial_putc('\r');
		s++;
	}
	spinlock_release(&_lock, fl);

#if CONFIG_INSTRUMENT
	instrument_enable();
#endif
}

