#include <twzthread.h>
#include <bstream.h>
#include <twzobj.h>
#include <twzname.h>

#include <debug.h>

static inline uint8_t inb(uint16_t port)
{
    uint8_t ret;
    asm volatile ( "inb %1, %0"
                   : "=a"(ret)
                   : "Nd"(port) );
    return ret;
}

static inline void outb(uint16_t port, uint8_t val)
{
    asm volatile ( "outb %0, %1" : : "a"(val), "Nd"(port) );
}

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
	outb(u->port + reg, value);
}

static uint8_t uart_read(struct uart *u, int reg)
{
	return inb(u->port + reg);
}
static struct uart com1 = {
	.port = COM1_PORT,
	.irq = COM1_IRQ,
};

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

struct object ko;
struct object so;
static void _input_thrd(void *arg)
{
	debug_printf("term input - starting");
	sys_thrd_ctl(THRD_CTL_SET_IOPL, 3);
	for(;;) {
		int x = serial_getc();
		serial_putc(x);
		if(x == '\r') {
			serial_putc('\n');
		}
		bstream_putb(&ko, x, 0);
	}
}

int main()
{
	debug_printf("term - starting");

	objid_t koid;
	twz_object_new(&ko, &koid, 0, 0, 0);
	if(bstream_init(&ko, 12)) {
		debug_printf("Failed to create ko");
	}

	objid_t soid;
	twz_object_new(&so, &soid, 0, 0, 0);
	if(bstream_init(&so, 12)) {
		debug_printf("Failed to create so");
	}

	twz_name_assign(koid, "keyboard", NAME_RESOLVER_DEFAULT);
	twz_name_assign(soid, "screen", NAME_RESOLVER_DEFAULT);

	struct twzthread it;
	if(twz_thread_spawn(&it, _input_thrd, NULL, NULL, 0) < 0) {
		debug_printf("Failed to spawn input thread");
		return 1;
	}

	twz_thread_ready();
	for(;;) {
		char buf[128];
		memset(buf, 0, sizeof(buf));
		ssize_t r = bstream_read(&so, buf, 127, 0);
		if(r > 0) {
			for(ssize_t i = 0;i<r;i++) {
				if(buf[i] == '\n') serial_putc('\r');
				serial_putc(buf[i]);
			}
		}
	//	debug_printf("SOREAD: %ld <%s>\n", r, buf);
		
	}
}

