#include <libgen.h>
#include <stdlib.h>
#include <twz/bstream.h>
#include <twz/debug.h>
#include <twz/name.h>
#include <twz/obj.h>
#include <twz/thread.h>
#include <unistd.h>

#include "ssfn.h"

#define ES_NORM 0
#define ES_ESC 1
#define ES_CSI 2
#define ES_DONE 3

struct fb {
	struct object obj;
	struct object font;
	size_t gl_w, gl_h;
	ssfn_glyph_t *glyph_cache[256];
	unsigned char *back_buffer, *front_buffer;
	unsigned char *char_buffer;
	size_t x, y, max_x, max_y;
	size_t fbw, fbh, pitch, bpp;
	int spacing;
	ssfn_t ctx;
	int init, flip;

	unsigned esc_args[16];
	int esc_argc;
	int esc_state;
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

#include <assert.h>
static void draw_glyph(const struct fb *restrict fb,
  ssfn_glyph_t *restrict glyph,
  uint32_t fgcolor,
  int c)
{
	unsigned int x, y, i, m;
	uint32_t pen_x = fb->x * (fb->gl_w + fb->spacing);
	uint32_t pen_y = fb->y * fb->gl_h;
	/* align glyph properly, we may have received a vertical letter */

	if(glyph->adv_y)
		pen_x -= (int8_t)glyph->baseline;
	else
		pen_y -= ((int8_t)glyph->baseline - fb->gl_h / 2);

	uint32_t max_gy = fb->max_y * fb->gl_h - pen_y;
	uint32_t *buffer = (void *)fb->back_buffer;

	uint32_t bpmul = fb->pitch / 4;
	uint32_t set = 0xff | fgcolor;
	uint64_t start = rdtsc();

	switch(glyph->mode) {
		case SSFN_MODE_BITMAP:
			for(y = 0; y < glyph->h && y < max_gy; y++) {
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
			for(y = 0; y < glyph->h && y < max_gy; y++) {
				uint32_t ys = (pen_y + y) * bpmul;
				uint32_t ygs = y * glyph->pitch;
				for(x = 0; x < glyph->w; x++) {
					buffer[ys + pen_x + x] = ARGB_TO_BGR((glyph->data[ygs + x] << 24) | fgcolor);
				}
			}
			break;

		case SSFN_MODE_CMAP:
			for(y = 0; y < glyph->h && y < max_gy; y++) {
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
	memmove(fb->back_buffer, fb->back_buffer + fb->pitch * nlines, fb->pitch * (fb->fbh - nlines));
	memset(fb->back_buffer + fb->pitch * (fb->fbh - nlines), 0, fb->pitch * nlines);
	fb->flip = 1;
}

void fb_clear_cell(struct fb *fb, int x, int y)
{
	uint32_t *px = (uint32_t *)fb->back_buffer;
	size_t pen_x = x * (fb->gl_w + fb->spacing);
	size_t yinc = fb->pitch / 4;
	size_t pen_y = fb->y * (fb->gl_h) * yinc;
	for(size_t y = pen_y; y < (pen_y + fb->gl_h * yinc); y += yinc) {
		for(size_t x = 0; x < fb->gl_w; x++) {
			px[y + pen_x + x] = 0;
		}
	}
}

void fb_clear_line(struct fb *fb, int line, int start, int end)
{
	uint32_t *px = (uint32_t *)fb->back_buffer;
	size_t pen_x = start * (fb->gl_w + fb->spacing);
	size_t yinc = fb->pitch / 4;
	size_t pen_y = line * (fb->gl_h) * yinc;
	unsigned int wid = (end - start) * (fb->gl_w + fb->spacing);
	for(size_t y = pen_y; y < (pen_y + fb->gl_h * yinc); y += yinc) {
		for(size_t x = 0; x < wid; x++) {
			px[y + pen_x + x] = 0;
		}
	}
}

#include <string.h>
void fb_del_chars(struct fb *fb, int count)
{
	if(fb->x + count > fb->max_x)
		count = fb->max_x - fb->x;
	uint32_t *px = (uint32_t *)fb->back_buffer;
	size_t pen_x = fb->x * (fb->gl_w + fb->spacing);
	size_t pen2_x = (fb->x + count) * (fb->gl_w + fb->spacing);
	size_t yinc = fb->pitch / 4;
	size_t pen_y = fb->y * (fb->gl_h) * yinc;
	for(size_t y = pen_y; y < (pen_y + fb->gl_h * yinc); y += yinc) {
		memmove(&px[y + pen_x], &px[y + pen2_x], fb->pitch - (fb->fbw - pen2_x) * fb->bpp);
	}
	fb_clear_line(fb, fb->y, fb->max_x - count, fb->max_x);
}

void fb_clear_screen(struct fb *fb, int sl, int el)
{
	for(int i = sl; i < el; i++) {
		fb_clear_line(fb, i, 0, fb->max_x);
	}
}

void fb_render_cursor(struct fb *fb)
{
	uint32_t *px = (uint32_t *)fb->front_buffer;
	uint32_t *pxb = (uint32_t *)fb->back_buffer;
	size_t pen_x = fb->x * (fb->gl_w + fb->spacing);
	size_t yinc = fb->pitch / 4;
	size_t pen_y = fb->y * (fb->gl_h) * yinc;
	for(size_t y = pen_y; y < (pen_y + (fb->gl_h - 3) * yinc); y += yinc) {
		for(size_t x = 0; x < fb->gl_w; x++) {
			px[y + pen_x + x + 1] = ~pxb[y + pen_x + x + 1];
		}
	}
}

void fb_render(struct fb *fb, int c)
{
	if(c < 8)
		return;
	fb->flip = 1;
	switch(c) {
		ssfn_glyph_t *glyph;
		case '\n':
			fb->y++;
			if(!(termios->c_oflag & ONLRET))
				break;
			/* fall-through */
		case '\r':
			fb->x = 0;
			break;
		case '\b':
			if(fb->x > 0) {
				fb->x--;
				/* TODO: check if we should erase or not */
				if((termios->c_lflag & ICANON) && (termios->c_lflag & ECHOE)) {
					fb_clear_cell(fb, fb->x, fb->y);
				}
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

			if(fb->x >= fb->max_x) {
				fb->x = 0;
				fb->y++;
			}

			if(fb->y >= fb->max_y) {
				fb_scroll(fb, fb->gl_h);
				fb->y--;
			}

			/* display the bitmap on your screen */
			fb_clear_cell(fb, fb->x, fb->y);
			draw_glyph(fb, glyph, 0x00ffffff, c);
			fb->x++;
			fb->flip = 1;
			break;
	}
}

struct termios def_term = {
	.c_iflag = BRKINT | ICRNL,
	.c_oflag = ONLCR | OPOST,
	.c_lflag = ICANON | ECHO | ISIG | ECHOE,
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

void init_fb(struct fb *fb)
{
	if(fb->init == 1) {
		struct pcie_function_header *hdr = twz_obj_base(&fb->obj);
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
		fb->gl_w = 8;
		fb->gl_h = 16;
		fb->x = 0;
		fb->y = 0;
		fb->spacing = 1;
		fb->max_x = fb->fbw / (fb->gl_w + fb->spacing);
		fb->max_y = fb->fbh / fb->gl_h;
		for(int i = 0; i < 256; i++)
			fb->glyph_cache[i] = NULL;

		fb->back_buffer = malloc(fb->pitch * fb->fbh);

		memset(fb->back_buffer, 0, fb->pitch * fb->fbh);
		fb->char_buffer = calloc(sizeof(fb->char_buffer[0]), fb->max_x * fb->max_y);

		struct object font;
		int r;
		if((r = twz_object_open_name(&font, "/usr/share/inconsolata.sfn", FE_READ))) {
			printf("ERR opening font: %d\n", r);
			fb->init = 0;
			return;
		}
		memset(&fb->ctx, 0, sizeof(fb->ctx));
		ssfn_font_t *_font_start = twz_obj_base(&font);

		if((r = ssfn_load(&fb->ctx, _font_start))) {
			debug_printf("load:%d\n", r);
			fb->init = 0;
			return;
		}

		if((r = ssfn_select(&fb->ctx,
		      SSFN_FAMILY_ANY,
		      NULL, /* family */
		      SSFN_STYLE_BOLD,
		      fb->gl_h,       /* style and size */
		      SSFN_MODE_ALPHA /* rendering mode */
		      ))) {
			debug_printf("select: %d\n", r);
			fb->init = 0;
			return;
		}
		fb->init = 2;
		setup_pty(fb->max_x, fb->max_y);
	}
}

void process_esc(struct fb *fb, int c)
{
	debug_printf(" -- unhandled ESC %c\n", c);
}

#include <ctype.h>
void process_csi(struct fb *fb, int c)
{
	if(isdigit(c)) {
		fb->esc_args[fb->esc_argc] *= 10;
		fb->esc_args[fb->esc_argc] += c - '0';
	} else if(c == ';') {
		fb->esc_argc++;
	} else {
		debug_printf("CSI seq %c (%d %d)\n", c, fb->esc_args[0], fb->esc_args[1]);
		switch(c) {
			case 'P':
				fb_del_chars(fb, fb->esc_args[0]);
				break;
			case 'C':
				if(fb->esc_args[0] == 0)
					fb->esc_args[0]++;
				fb->x += fb->esc_args[0];
				if(fb->x > fb->max_x)
					fb->x = fb->max_x;
				break;
			case 'K':
				switch(fb->esc_args[0]) {
					case 0:
						fb_clear_line(fb, fb->y, fb->x, fb->max_x);
						break;
					case 1:
						fb_clear_line(fb, fb->y, 0, fb->x + 1);
						break;
					case 2:
						fb_clear_line(fb, fb->y, 0, fb->max_x);
						break;
				}
				break;
			case 'J':
				/* TODO: does this clear the entire line the cursor is on, or no? */
				switch(fb->esc_args[0]) {
					case 0:
						fb_clear_screen(fb, fb->y, fb->max_y);
						break;
					case 1:
						fb_clear_screen(fb, 0, fb->y + 1);
						break;
					case 2:
					case 3:
						fb_clear_screen(fb, 0, fb->max_y);
						break;
					default:
						debug_printf("bad arg to CSI J: %d\n", fb->esc_args[0]);
				}
				break;
			case 'H':
				/* args are 1-based, but an ommitted arg is a zero, so just switch everything to
				 * zero-based, which is better anyway */
				if(fb->esc_args[0])
					fb->esc_args[0]--;
				if(fb->esc_args[1])
					fb->esc_args[1]--;

				if(fb->esc_args[0] > fb->max_y)
					fb->esc_args[0] = fb->max_y;
				if(fb->esc_args[1] > fb->max_x)
					fb->esc_args[1] = fb->max_x;

				fb->x = fb->esc_args[1];
				fb->y = fb->esc_args[0];
				debug_printf(" got H CSI: %d %d\n", fb->x, fb->y);

				break;
			default:
				debug_printf(" -- unhandled CSI %c (%d %d %d)\n",
				  c,
				  fb->esc_args[0],
				  fb->esc_args[1],
				  fb->esc_args[2]);
		}
		fb->esc_state = ES_DONE;
		fb->flip = 1;
	}
}

void process_incoming(struct fb *fb, int c)
{
	switch(fb->esc_state) {
		case ES_NORM:
			if(c == 27)
				fb->esc_state = ES_ESC;
			break;
		case ES_ESC:
			fb->esc_state = ES_DONE;
			if(c == '[') {
				memset(fb->esc_args, 0, sizeof(fb->esc_args));
				fb->esc_argc = 0;
				fb->esc_state = ES_CSI;
			} else
				process_esc(fb, c);
			break;
		case ES_CSI:
			process_csi(fb, c);
			break;
	}

	if(fb->esc_state == ES_NORM) {
		// if(c != '\n' && c != '\r')
		//	fb->char_buffer[fb->y * fb->max_x + fb->x] = c;
		fb_render(fb, c);
	} else if(fb->esc_state == ES_DONE) {
		fb->esc_state = ES_NORM;
	}
}

void fb_putc(struct fb *fb, int c)
{
	if(fb->init == 0)
		return;
	init_fb(fb);
	fb->flip = 0;
	if(c > 0xff || !c) {
		return;
	}

	process_incoming(fb, c);

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
