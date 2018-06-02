#include <arch/x86_64-io.h>
#include <processor.h>
#include <instrument.h>
#include <debug.h>
#include <interrupt.h>
#define PORT 0x3f8   /* COM1 */

void serial_init()
{
	for(int i=1;i<8;i++) {
		x86_64_inb(PORT + i);
	}
	x86_64_inb(PORT);

	x86_64_outb(PORT + 1, 0);
	x86_64_outb(PORT + 3, 0x80); // set up to load divisor latch
	x86_64_outb(PORT + 0, 3); // lsb
	x86_64_outb(PORT + 1, 0); // msb
	x86_64_outb(PORT + 3, 3); // 8N1
	x86_64_outb(PORT + 1, 0x1);
	x86_64_outb(PORT + 2, 0x07); // enable FIFO, clear, 1-byte threshold
	x86_64_outb(PORT + 4, 1);
	x86_64_outb(PORT + 4, 1 | 2);
	while((x86_64_inb(PORT + 2) & 1) == 0);

	for(int i=1;i<8;i++) {
		x86_64_inb(PORT + i);
	}
	x86_64_inb(PORT);
	x86_64_outb(PORT + 4, 1 | 2 | 4 | 8);
}

static void _serial_interrupt(int i)
{
	(void)i;
	printk("RESET\n");
	uint32_t isr = x86_64_inb(PORT + 2);
	printk(":: %d\n", isr);
	arch_processor_reset();
}

int serial_getc(void)
{
	return x86_64_inb(PORT);
}

int serial_received(void)
{
	uint32_t isr = x86_64_inb(PORT + 2);
	printk(":: %d\n", isr);
	return x86_64_inb(PORT + 5) & 1;
}

static struct interrupt_handler _serial_handler = {
	.fn = _serial_interrupt,
};

__initializer static void _init_serial(void)
{
	int a = 0x24;
	interrupt_register_handler(a, &_serial_handler);
	arch_interrupt_unmask(a);
	arch_interrupt_unmask(9+32);
	printk(":: %d %d\n", serial_received(), serial_getc());
	uint32_t isr = x86_64_inb(PORT + 2);
	printk(":: %d\n", isr);
}

static int is_transmit_empty()
{
	return x86_64_inb(PORT + 5) & 0x20;
}

static void serial_putc(unsigned char byte)
{
	while (is_transmit_empty() == 0);
	x86_64_outb(PORT, byte);
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

