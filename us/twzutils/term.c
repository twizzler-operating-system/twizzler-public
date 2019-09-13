#include <libgen.h>
#include <stdlib.h>
#include <twz/bstream.h>
#include <twz/debug.h>
#include <twz/name.h>
#include <twz/obj.h>
#include <twz/thread.h>
#include <unistd.h>

#include "ssfn.h"
struct fb {
	struct object obj;
	struct object font;
	size_t fsz;
	ssfn_glyph_t *glyph_cache[256];
	unsigned char *back_buffer, *front_buffer;
	unsigned char *char_buffer;
	size_t cw, ch, cx, cy, fbx, fby;
	size_t fbw, fbh, pitch, bpp;
	ssfn_t ctx;
	int init, flip;
};

struct fb fb = { 0 };

void fb_putc(struct fb *fb, int c);

static struct object ptyobj, kbobj;
static struct termios *termios;

#include <twz/io.h>

void process_keyboard(struct object *, char *, size_t);

void kbmain(void *a)
{
	snprintf(twz_thread_repr_base()->hdr.name, KSO_NAME_MAXLEN, "[instance] term.input");

	sys_thrd_ctl(THRD_CTL_SET_IOPL, 3);
	int r;
	if((r = twz_thread_ready(NULL, THRD_SYNC_READY, 0))) {
		debug_printf("failed to mark ready");
		abort();
	}

	for(;;) {
		int x;
		char buf[128];
		ssize_t r = twzio_read(&kbobj, buf, 128, 0, 0);
		if(r < 0) {
			debug_printf("ERR: %ld\n", r);
			twz_thread_exit();
		}
		process_keyboard(&ptyobj, buf, r);
	}
}

#include <twz/driver/pcie.h>

struct __packed bga_regs {
	uint16_t index;
	uint16_t xres;
	uint16_t yres;
	uint16_t bpp;
	uint16_t enable;
	uint16_t bank;
	uint16_t vwidth;
	uint16_t vheight;
	uint16_t xoff;
	uint16_t yoff;
};
#define SDL_PIXEL ((uint32_t *)(fb->back_buffer))[(pen_y + y) * fb->pitch / 4 + (pen_x + x)]

static inline uint32_t ARGB_TO_BGR(uint32_t x)
{
	uint32_t a = (x >> 24) & 0xff;
	uint32_t r = (x >> 16) & 0xff;
	uint32_t g = (x >> 8) & 0xff;
	uint32_t b = x & 0xff;
	r *= a;
	r /= 0xff;
	g *= a;
	g /= 0xff;
	b *= a;
	b /= 0xff;
	return (r << 16 | g << 8 | b);
}

static __inline__ unsigned long long rdtsc(void)
{
	unsigned hi, lo;
	__asm__ __volatile__("rdtscp" : "=a"(lo), "=d"(hi));
	return ((unsigned long long)lo) | (((unsigned long long)hi) << 32);
}

static void draw_glyph(const struct fb *restrict fb,
  ssfn_glyph_t *restrict glyph,
  uint32_t fgcolor,
  int c)
{
	unsigned int x, y, i, m;
	uint32_t pen_x = fb->fbx;
	uint32_t pen_y = fb->fby;
	/* align glyph properly, we may have received a vertical letter */

	if(glyph->adv_y)
		pen_x -= (int8_t)glyph->baseline;
	else
		pen_y -= (int8_t)glyph->baseline;

	uint32_t *buffer = (void *)fb->back_buffer;

	uint32_t bpmul = fb->pitch / 4;
	uint32_t set = 0xff | fgcolor;
	uint64_t start = rdtsc();

	switch(glyph->mode) {
		case SSFN_MODE_BITMAP:
			for(y = 0; y < glyph->h; y++) {
				uint32_t ys = (pen_y + y) * bpmul;
				uint32_t ygs = y * glyph->pitch;
				for(x = 0, i = 0, m = 1; x < glyph->w; x++, m <<= 1) {
					if(m > 0x80) {
						m = 1;
						i++;
					}
					buffer[ys + pen_x + x] = (glyph->data[ygs + i] & m) ? set : 0;
				}
			}
			break;

		case SSFN_MODE_ALPHA:
			for(y = 0; y < glyph->h; y++)
				for(x = 0; x < glyph->w; x++) {
					uint32_t pix = (fgcolor & 0xff);
					pix *= glyph->data[y * glyph->pitch + x];
					pix /= 0xff;
					pix |= pix << 8;
					pix |= pix << 8;
					SDL_PIXEL = pix;
					// SDL_PIXEL = (uint32_t)((glyph->data[y * glyph->pitch + x] << 24) | fgcolor);
				}
			break;

		case SSFN_MODE_CMAP:
			for(y = 0; y < glyph->h; y++) {
				uint32_t ys = (pen_y + y) * bpmul;
				uint32_t ygs = y * glyph->pitch;
				for(x = 0; x < glyph->w; x++) {
					buffer[ys + pen_x + x] =
					  ARGB_TO_BGR(SSFN_CMAP_TO_ARGB(glyph->data[ygs + x], glyph->cmap, fgcolor));
				}
			}
			break;
		default:
			debug_printf("Unsupported mode for %x: %d\n", c, glyph->mode);
	}
	uint64_t end = rdtsc();
	// debug_printf("TSC: %ld\n", end - start);
}

#if 0
#include <immintrin.h>
/* ... */
void fastMemcpy(void *pvDest, void *pvSrc, size_t nBytes)
{
	const __m256i *pSrc = (const __m256i *)(pvSrc);
	__m256i *pDest = (__m256i *)(pvDest);
	int64_t nVects = nBytes / sizeof(*pSrc);
	for(; nVects > 0; nVects--, pSrc++, pDest++) {
		const __m256i loaded = _mm256_stream_load_si256(pSrc);
		_mm256_stream_si256(pDest, loaded);
	}
	_mm_sfence();
}
#endif

#include <sys/ioctl.h>
#include <termios.h>
#include <twz/pty.h>
void fb_scroll(struct fb *fb, int nlines)
{
	// memmove(&fb->char_buffer[0], &fb->char_buffer[fb->cw], fb->cw * (fb->ch - 1));
	// memset(&fb->char_buffer[fb->cw * (fb->ch - 1)], 0, fb->cw);
	memcpy(fb->back_buffer, fb->back_buffer + fb->pitch * nlines, fb->pitch * (fb->fbh - nlines));
	memset(fb->back_buffer + fb->pitch * (fb->fbh - nlines), 0, fb->pitch * nlines);
	fb->flip = 1;
}

void fb_render_cursor(struct fb *fb)
{
	/* write directly to the front buffer */
	uint32_t *px = (uint32_t *)fb->front_buffer;
	for(size_t y = 0; y < fb->fsz - 3; y++) {
		px[(y + fb->fby - fb->fsz / 2) * fb->pitch / 4 + fb->fbx + 1] = 0x00ffffff;
	}
}

void fb_render(struct fb *fb, int c)
{
	switch(c) {
		ssfn_glyph_t *glyph;
		case '\n':
			fb->fby += fb->fsz;
			if(!(termios->c_oflag & ONLRET))
				break;
			/* fall-through */
		case '\r':
			fb->fbx = 0;
			break;
		case '\b':
			if(fb->fbx > 0) {
				fb->fbx -= 8;
				fb_render(fb, ' ');
				fb->fbx -= 8;
			}
			break;
		default:
			glyph = fb->glyph_cache[c];
			if(!glyph) {
				glyph = fb->glyph_cache[c] = ssfn_render(&fb->ctx, c);
			}

			if(!glyph) {
				if(ssfn_lasterr(&fb->ctx)) {
					debug_printf("render %x: %d\n", c, ssfn_lasterr(&fb->ctx));
				}
				return;
			}

			if(fb->fbx + glyph->adv_x >= fb->fbw) {
				fb->fbx = 0;
				fb->fby += fb->fsz;
			}

			long lines = fb->fbh;
			lines -= (fb->fby + fb->fsz);
			if(lines < 0) {
				fb_scroll(fb, -lines);
				fb->fby += lines;
			}

			/* display the bitmap on your screen */
			draw_glyph(fb, glyph, 0x00ffffff, c);
			fb->fbx += glyph->adv_x;
			fb->flip = 1;
			break;
	}
}

struct termios def_term = {
	.c_iflag = BRKINT | ICRNL,
	.c_oflag = ONLCR | OPOST,
	.c_lflag = ICANON | ECHO | ISIG,
	.c_cc[VEOF] = 4,
	.c_cc[VEOL] = 0,
	.c_cc[VERASE] = '\b',
	.c_cc[VINTR] = 3,
	.c_cc[VMIN] = 1,
	.c_cc[VSUSP] = 26,
};

void setup_pty(int cw, int ch)
{
	struct winsize w = { .ws_row = ch, .ws_col = cw };
	int r;
	if((r = twzio_ioctl(&ptyobj, TCSETS, &def_term)) < 0) {
		fprintf(stderr, "failed to set pty termios: %d\n", r);
	}
	if((r = twzio_ioctl(&ptyobj, TIOCSWINSZ, &w)) < 0) {
		fprintf(stderr, "failed to set pty winsize: %d\n", r);
	}
	struct pty_hdr *hdr = twz_obj_base(&ptyobj);
	termios = &hdr->termios;
}

void fb_putc(struct fb *fb, int c)
{
	if(fb->init == 0)
		return;
	struct pcie_function_header *hdr = twz_obj_base(&fb->obj);
	if(fb->init == 1) {
		fb->front_buffer = twz_ptr_lea(&fb->obj, (void *)hdr->bars[0]);
		volatile struct bga_regs *regs = twz_ptr_lea(&fb->obj, (void *)hdr->bars[2] + 0x500);
		regs->index = 0xb0c5;
		regs->enable = 0;
		regs->xres = 1024;
		regs->yres = 768;
		regs->bpp = 0x20;
		regs->enable = 1 | 0x40;

		fb->fbw = 1024;
		fb->fbh = 768;
		fb->bpp = 4;
		fb->pitch = fb->fbw * fb->bpp;
		fb->fsz = 16;
		fb->cw = fb->fbw / (fb->fsz / 4);
		fb->ch = fb->fbh / fb->fsz * 32;
		fb->fbx = 0;
		fb->fby = fb->fsz / 2;
		for(int i = 0; i < 256; i++)
			fb->glyph_cache[i] = NULL;

		fb->back_buffer = malloc(fb->pitch * fb->fbh);

		memset(fb->back_buffer, 0, fb->pitch * fb->fbh);
		fb->char_buffer = calloc(sizeof(fb->char_buffer[0]), fb->cw * fb->ch);

		struct object font;
		int r;
		if((r = twz_object_open_name(&font, "inconsolata.sfn", FE_READ))) {
			printf("ERR opening font: %d\n", r);
			fb->init = 0;
			return;
		}
		memset(&fb->ctx, 0, sizeof(fb->ctx));
		ssfn_font_t *_font_start = twz_obj_base(&font);

		if((r = ssfn_load(&fb->ctx, _font_start))) {
			debug_printf("load:%d\n", r);
			fb->init == 0;
			return;
		}

		if((r = ssfn_select(&fb->ctx,
		      SSFN_FAMILY_ANY,
		      NULL, /* family */
		      SSFN_STYLE_REGULAR,
		      fb->fsz,       /* style and size */
		      SSFN_MODE_CMAP /* rendering mode */
		      ))) {
			debug_printf("select: %d\n", r);
			fb->init = 0;
			return;
		}
		fb->init = 2;
		setup_pty(fb->cw, fb->ch);
	}

	fb->flip = 0;
	if(c > 0xff || !c) {
		return;
	}

	if(c != '\n' && c != '\r')
		fb->char_buffer[fb->cw * fb->cy + fb->cx] = c;
	fb_render(fb, c);

	uint64_t s = rdtsc();
	if(fb->flip) {
		// fastMemcpy(fb->front_buffer, fb->back_buffer, fb->fbh * fb->pitch);
		memcpy(fb->front_buffer, fb->back_buffer, fb->fbh * fb->pitch);
		// fb->front_buffer[i] = 0; // fb->back_buffer[i];
		// x += fb->back_buffer[i];
	}
	fb_render_cursor(fb);

	uint64_t e = rdtsc();
	// debug_printf("flip: %ld\n", e - s);
}

void curfb_putc(int c)
{
	fb_putc(&fb, c);
}
int main(int argc, char **argv)
{
	int r;

	char *input, *output, *pty;

	int c;
	while((c = getopt(argc, argv, "i:o:p:")) != -1) {
		switch(c) {
			case 'i':
				input = optarg;
				break;
			case 'o':
				output = optarg;
				break;
			case 'p':
				pty = optarg;
				break;
			default:
				fprintf(stderr, "unrecognized option: %c\n", c);
				exit(1);
		}
	}

	twz_object_open_name(&kbobj, input, FE_READ | FE_WRITE);
	twz_object_open_name(&ptyobj, pty, FE_READ | FE_WRITE);

#if 0
	objid_t kid;
	if((r = twz_object_create(TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE, 0, 0, &kid))) {
		debug_printf("failed to create screen object");
		abort();
	}

	if((r = twz_object_open(&out_obj, kid, FE_READ | FE_WRITE))) {
		debug_printf("failed to open screen object");
		abort();
	}

	if((r = bstream_obj_init(&out_obj, twz_obj_base(&out_obj), 16))) {
		debug_printf("failed to init screen bstream");
		abort();
	}

	if((r = twz_name_assign(kid, "dev:dfl:input"))) {
		debug_printf("failed to assign screen object name");
		abort();
	}

	objid_t sid;
	if((r = twz_object_create(TWZ_OC_DFL_READ | TWZ_OC_DFL_WRITE, 0, 0, &sid))) {
		debug_printf("failed to create screen object");
		abort();
	}

	if((r = twz_object_open(&screen_obj, sid, FE_READ | FE_WRITE))) {
		debug_printf("failed to open screen object");
		abort();
	}

	if((r = bstream_obj_init(&screen_obj, twz_obj_base(&screen_obj), 16))) {
		debug_printf("failed to init screen bstream");
		abort();
	}

	if((r = twz_name_assign(sid, "dev:dfl:screen"))) {
		debug_printf("failed to assign screen object name");
		abort();
	}
#endif

	fb.init = 1;
	if((r = twz_object_open_name(&fb.obj, output, FE_READ | FE_WRITE))) {
		debug_printf("term: failed to open framebuffer: %d\n", r);
		fb.init = 0;
	}

	fb_putc(&fb, 0);
	struct thread kthr;
	if((r = twz_thread_spawn(
	      &kthr, &(struct thrd_spawn_args){ .start_func = kbmain, .arg = NULL }))) {
		debug_printf("failed to spawn kb thread");
		abort();
	}

	twz_thread_wait(1, (struct thread *[]){ &kthr }, (int[]){ THRD_SYNC_READY }, NULL, NULL);

	if((r = twz_thread_ready(NULL, THRD_SYNC_READY, 0))) {
		debug_printf("failed to mark ready");
		abort();
	}

	for(;;) {
		char buf[128];
		memset(buf, 0, sizeof(buf));
		ssize_t r = twzio_read(&ptyobj, buf, 127, 0, 0);
		if(r > 0) {
			for(ssize_t i = 0; i < r; i++) {
				if(fb.init) {
					if(buf[i] == '\n')
						fb_putc(&fb, '\r');
					fb_putc(&fb, buf[i]);
				}
			}
		}
	}
}
