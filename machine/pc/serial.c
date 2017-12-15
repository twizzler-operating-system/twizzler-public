#include <arch/x86_64-io.h>
#define PORT 0x3f8   /* COM1 */

void serial_init()
{
    x86_64_outb(PORT + 3, 0x80); // set up to load divisor latch
    x86_64_outb(PORT + 0, 2); // lsb
    x86_64_outb(PORT + 1, 0); // msb
    x86_64_outb(PORT + 3, 3); // 8N1
    x86_64_outb(PORT + 2, 0x07); // enable FIFO, clear, 14-byte threshold
	x86_64_outb(PORT + 1, 0x01);
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
	bool fl = spinlock_acquire(&_lock);
	while(*s) {
		serial_putc(*s);
		if(*s == '\n')
			serial_putc('\r');
		s++;
	}
	spinlock_release(&_lock, fl);
}

