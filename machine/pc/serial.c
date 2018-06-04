#include <arch/x86_64-io.h>
#include <processor.h>
#include <instrument.h>
#include <debug.h>
#include <interrupt.h>
#define COM1_PORT 0x3f8   /* COM1 */
#define COM1_IRQ 0x24

#define UART_REG_DATA    0
#define UART_REG_IER     1
#define UART_REG_IID     2
#define UART_REG_FIFOCTL 2 /* same as above */
#define UART_REG_LCR     3
#define UART_REG_MCR     4
#define UART_REG_LSR     5
#define UART_REG_MSR     6
#define UART_REG_SCRATCH 7

#define UART_REG_DIV_LSB 0 /* with DLAB set to 1 */
#define UART_REG_DIV_MSB 1

#define UART_IER_RX_FULL    (1 << 0)
#define UART_IER_TX_FULL    (1 << 1)
#define UART_IER_LINESTATUS (1 << 2)
#define UART_IER_LINEDELTA  (1 << 3)

#define UART_IID_FIFOEN1 (1 << 7)
#define UART_IID_FIFOEN2 (1 << 6)
#define UART_IID_IID2    (1 << 3)
#define UART_IID_IID1    (1 << 2)
#define UART_IID_IID0    (1 << 1)
#define UART_IID_PENDING (1 << 0)

#define UART_FCR_FIFO64   (1 << 5)
#define UART_FCR_DMASEL   (1 << 3)
#define UART_FCR_TX_RESET (1 << 2)
#define UART_FCR_RX_RESET (1 << 1)
#define UART_FCR_ENABLE   (1 << 0)
/* bits 6,7 are RX trigger */

#define UART_LCR_DLAB       (1 << 7)
#define UART_LCR_SBR        (1 << 6)
#define UART_LCR_STICKY_PAR (1 << 5)
#define UART_LCR_EVEN_PAR   (1 << 4)
#define UART_LCR_ENABLE_PAR (1 << 3)
#define UART_LCR_STOP_BIT   (1 << 2)
/* bits 0,1 are word length */

#define UART_MCR_LOOP_EANBLE (1 << 4)
#define UART_MCR_OUT2 (1 << 3)
#define UART_MCR_OUT1 (1 << 2)
#define UART_MCR_RTS  (1 << 1)
#define UART_MCR_DTR  (1 << 0)

#define UART_LSR_FIFO_ERR    (1 << 7)
#define UART_LSR_TX_EMPTY    (1 << 6)
#define UART_LSR_THR_EMPTY   (1 << 5)
#define UART_LSR_BREAK       (1 << 4)
#define UART_LSR_FRAME_ERR   (1 << 3)
#define UART_LSR_PARITY_ERR  (1 << 2)
#define UART_LSR_OVERRUN_ERR (1 << 1)
#define UART_LSR_RX_READY    (1 << 0)

#define UART_MSR_DCD  (1 << 7)
#define UART_MSR_RI   (1 << 6)
#define UART_MSR_DSR  (1 << 5)
#define UART_MSR_CTS  (1 << 4)
#define UART_MSR_DDCD (1 << 3)
#define UART_MSR_TERI (1 << 2)
#define UART_MSR_DDSR (1 << 1)
#define UART_MSR_DCTS (1 << 0)

#define UART_PARITY_TYPE_NONE  0
#define UART_PARITY_TYPE_ODD   1
#define UART_PARITY_TYPE_EVEN  2
#define UART_PARITY_TYPE_MARK  3
#define UART_PARITY_TYPE_SPACE 4

struct uart {
	uint8_t lc;
	uint8_t mc;
	uint8_t ie;
	uint8_t fc;
	int divisor;
	int port;
	int max_baud;
	int fifo_sz;
	int irq;
};

static void uart_write(struct uart *u, int reg, uint8_t value)
{
	x86_64_outb(u->port + reg, value);
}

static uint8_t uart_read(struct uart *u, int reg)
{
	return x86_64_inb(u->port + reg);
}

static void uart_configure_line(struct uart *u, int parity, int stopbits, int wordlen)
{
	uint8_t lc = 0;
	switch(parity) {
		case UART_PARITY_TYPE_EVEN: lc |= UART_LCR_EVEN_PAR; break;
		case UART_PARITY_TYPE_SPACE: lc |= UART_LCR_EVEN_PAR; break;
		case UART_PARITY_TYPE_MARK: lc |= UART_LCR_STICKY_PAR | UART_LCR_EVEN_PAR; break;
	}
	if(parity != UART_PARITY_TYPE_NONE) {
		lc |= UART_LCR_ENABLE_PAR;
	}
	if(stopbits == 2) {
		lc |= UART_LCR_STOP_BIT;
	}
	lc |= (wordlen - 5) & 3;

	u->lc = lc;
}

static void uart_set_speed(struct uart *u, int speed)
{
	if(speed > u->max_baud) {
		speed = u->max_baud;
	}
	u->divisor = 115200 / speed;
}

static void uart_set_fifo(struct uart *u, int fifosz)
{
	u->fifo_sz = fifosz;
	if(u->fifo_sz > 1) {
		u->fc = UART_FCR_ENABLE | UART_FCR_TX_RESET | UART_FCR_RX_RESET;
		switch(u->fifo_sz) {
			case 4: u->fc |= (1 << 6); break;
			case 8: u->fc |= (1 << 7); break;
			default: u->fc |= (1 << 6) | (1 << 7); break;
		}
	}
}

static void uart_reset(struct uart *u)
{
	uart_read(u, UART_REG_IER);
	uart_read(u, UART_REG_IID);
	uart_read(u, UART_REG_LCR);
	uart_read(u, UART_REG_MCR);
	uart_read(u, UART_REG_LSR);
	uart_read(u, UART_REG_MSR);
	uart_read(u, UART_REG_SCRATCH);
	uart_read(u, UART_REG_DATA);
}

static void uart_program(struct uart *u, bool interrupts)
{
	/* We don't know how the BIOS left the UARTs. Read all the registers to ack
	 * any pending info and interrupts */
	uart_reset(u);

	uart_write(u, UART_REG_LCR, UART_LCR_DLAB | u->lc);
	uart_write(u, UART_REG_DIV_LSB, u->divisor & 0xFF);
	uart_write(u, UART_REG_DIV_LSB, (u->divisor >> 8) & 0xFF);
	uart_write(u, UART_REG_LCR, u->lc);

	uart_write(u, UART_REG_FIFOCTL, u->fc);

	if(interrupts) {
		u->ie = UART_IER_RX_FULL | UART_IER_TX_FULL
			| UART_IER_LINESTATUS | UART_IER_LINEDELTA;
		//u->ie = UART_IER_RX_FULL
		//	| UART_IER_LINESTATUS | UART_IER_LINEDELTA;
	} else {
		u->ie = 0;
	}
	uart_write(u, UART_REG_IER, u->ie);
	
	u->mc = UART_MCR_DTR | UART_MCR_RTS;
	uart_write(u, UART_REG_MCR, u->mc);

	/* ack everything, because funky hardware */
	uart_reset(u);

	if(interrupts) {
		u->mc |= UART_MCR_OUT1 | UART_MCR_OUT2;
		uart_write(u, UART_REG_MCR, u->mc);
	}
}

static void uart_identify(struct uart *u)
{
	uart_reset(u);

	uint8_t fc = 0;
	fc |= UART_FCR_ENABLE;
	fc |= UART_FCR_TX_RESET;
	fc |= UART_FCR_RX_RESET;
	fc |= (3 << 6);
	fc |= UART_FCR_FIFO64;
	uart_write(u, UART_REG_FIFOCTL, fc);
	fc = uart_read(u, UART_REG_IID);
	int trigger = (fc >> 6) & 3;
	int max_baud, fifo_size;
	if(trigger == 3 && (fc & UART_FCR_FIFO64)) {
		/* We can write to the 64-byte setting. It's a 16750. */
		fifo_size = 64;
		max_baud = 115200;
	} else if(trigger == 3 && !(fc & UART_FCR_FIFO64)) {
		/* We cannot write to the 64-byte setting, but both trigger bits
		 * were writable. It's a 16550A. */
		fifo_size = 14;
		max_baud = 115200;
	} else if(trigger != 3) {
		/* Detected the buggy 16550. Fifo cannot be used */
		fifo_size = 1;
		max_baud = 57600;
	} else {
		/* The 16450 preserved the scratch register */
		uart_write(u, UART_REG_SCRATCH, 0xAA);
		if(uart_read(u, UART_REG_SCRATCH) == 0xAA) {
			max_baud = 38400;
		} else {
			max_baud = 19200;
		}
		fifo_size = 1;
	}
	
	u->max_baud = max_baud;
	uart_set_fifo(u, fifo_size);
}

static void uart_configure(struct uart *u, bool interrupts, int parity,
		int stop, int word, int speed) {
	uart_set_speed(u, speed);
	uart_configure_line(u, parity, stop, word);
	uart_program(u, interrupts);
}

static void uart_init(struct uart *u, bool interrupts, int parity, int stop, int word, int speed)
{
	uart_identify(u);
	uart_configure(u, interrupts, parity, stop, word, speed);
}

static struct uart com1 = {
	.port = COM1_PORT,
	.irq = COM1_IRQ,
};

static void _serial_interrupt(int i)
{
	struct uart *u = &com1;
	(void)i;
	uint8_t id;
	while(((id = uart_read(u, UART_REG_IID)) & 1) == 0) {
		switch((id >> 1) & 7) {
			case 0:
				uart_read(u, UART_REG_MSR);
				break;
			case 1:
				break;
			case 2: case 6:
				panic("DEBUG RESET");
				uart_read(u, UART_REG_DATA);
				break;
			case 3:
				uart_read(u, UART_REG_LSR);
				break;
		}
	}
}

static struct interrupt_handler _serial_handler = {
	.fn = _serial_interrupt,
};

void serial_init(void)
{
	uart_init(&com1, false, UART_PARITY_TYPE_NONE, 1, 8, 38400);
	printk("Initialized serial debugging (max_baud=%d, fifo_sz=%d)\n",
			com1.max_baud, com1.fifo_sz);
}

void serial_putc(char c)
{
	while((uart_read(&com1, UART_REG_LSR) & UART_LSR_THR_EMPTY) == 0)
		;
	uart_write(&com1, UART_REG_DATA, c);
}

char serial_getc()
{
	while((uart_read(&com1, UART_REG_LSR) & UART_LSR_RX_READY) == 0)
		;
	return uart_read(&com1, UART_REG_DATA);
}

__initializer static void __serial_init(void)
{
	interrupt_register_handler(com1.irq, &_serial_handler);
	arch_interrupt_unmask(com1.irq);
	uart_init(&com1, true, UART_PARITY_TYPE_NONE, 1, 8, 38400);
}

/*
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
*/

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

