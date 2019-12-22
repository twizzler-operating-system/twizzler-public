#include <arch/x86_64-io.h>
#include <arch/x86_64.h>

#define PIT_CHANNEL_BIT 6
#define PIT_ACCESS_BIT 4
#define PIT_MODE_BIT 1
#define PIT_FORMAT_BIT 0

#define PIT_CHANNEL(n) ((n) << PIT_CHANNEL_BIT)
#define PIT_READBACK PIT_CHANNEL(3)

#define PIT_ACCESS(n) ((n) << PIT_ACCESS_BIT)
#define PIT_ACCESS_LATCH PIT_ACCESS(0)
#define PIT_ACCESS_LO PIT_ACCESS(1)
#define PIT_ACCESS_HI PIT_ACCESS(2)
#define PIT_ACCESS_BOTH PIT_ACCESS(3)

#define PIT_MODE(n) ((n) << PIT_MODE_BIT)
#define PIT_MODE_IOTC PIT_MODE(0)
#define PIT_MODE_ONESHOT PIT_MODE(1)
#define PIT_MODE_RATEGEN PIT_MODE(2)
#define PIT_MODE_SQUARE PIT_MODE(3)
#define PIT_MODE_SOFTSTROBE PIT_MODE(4)
#define PIT_MODE_HARDSTROBE PIT_MODE(5)
#define PIT_MODE_RATE2 PIT_MODE(6)
#define PIT_MODE_SQUARE2 PIT_MODE(7)

#define PIT_FORMAT(n) ((n) << PIT_FORMAT_BIT)
#define PIT_FORMAT_BINARY PIT_FORMAT(0)
#define PIT_FORMAT_BCD PIT_FORMAT(1)
#define PIT_BASE 0x40
#define PIT_CMD (PIT_BASE + 3)
#define PIT_DATA(channel) (PIT_BASE + (channel))

uint64_t x86_64_early_wait_ns(int64_t ns)
{
	__int128 x = ns;
	x *= 1193182ul;
	int64_t count = (int64_t)(x / 1000000000ul);

	x86_64_outb(PIT_CMD, PIT_CHANNEL(2) | PIT_ACCESS_BOTH | PIT_MODE_ONESHOT | PIT_FORMAT_BINARY);

	uint64_t ec = 0;
	while(count > 64) {
		uint32_t readback;
		uint32_t thiscount = 0xFFFF;
		if(thiscount > count) {
			thiscount = count + 64;
			if(thiscount > 0xFFFF)
				thiscount = 0xFFFF;
		}

		x86_64_outb(PIT_DATA(2), thiscount & 0xFF);
		x86_64_outb(PIT_DATA(2), (thiscount >> 8) & 0xFF);
		/* force count to be reloaded */
		x86_64_outb(0x61, 0);
		x86_64_outb(0x61, 1);

		do {
			x86_64_outb(PIT_CMD, PIT_CHANNEL(2) | PIT_ACCESS_LATCH);
			readback = x86_64_inb(PIT_DATA(2));
			readback |= x86_64_inb(PIT_DATA(2)) << 8;
			asm("pause");
		} while(readback > 64);

		ec += (thiscount - readback);
		count -= (thiscount - readback);
	}
	x = ec;
	x *= 1000000000ul;
	return x / 1193182ul;
}
