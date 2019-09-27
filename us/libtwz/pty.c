#include <string.h>
#include <twz/event.h>
#include <twz/gate.h>
#include <twz/io.h>
#include <twz/name.h>
#include <twz/obj.h>
#include <twz/pty.h>

#include <errno.h>

#include <twz/debug.h>

#define PTY_NBITS 11

int pty_ioctl_server(struct object *obj, int request, long arg)
{
	int r = 0;
	struct pty_hdr *hdr = twz_obj_base(obj);
	switch(request) {
		case TCSETS:
		case TCSETSW:
			memcpy(&hdr->termios, (void *)arg, sizeof(hdr->termios));
			break;
		case TCGETS:
			memcpy((void *)arg, &hdr->termios, sizeof(hdr->termios));
			break;
		case TIOCSWINSZ:
			memcpy(&hdr->wsz, (void *)arg, sizeof(hdr->wsz));
			break;
		case TIOCGWINSZ:
			memcpy((void *)arg, &hdr->wsz, sizeof(hdr->wsz));
			break;
		default:
			r = -ENOTSUP;
			break;
	}
	return r;
}

int pty_ioctl_client(struct object *obj, int request, long arg)
{
	struct pty_client_hdr *hdr = twz_obj_base(obj);
	struct pty_hdr *sh = twz_ptr_lea(obj, hdr->server);
	struct object sh_obj = TWZ_OBJECT_FROM_PTR(sh);
	return pty_ioctl_server(&sh_obj, request, arg);
}

ssize_t pty_read_server(struct object *obj, void *ptr, size_t len, unsigned flags)
{
	struct pty_hdr *hdr = twz_obj_base(obj);
	return bstream_hdr_read(obj, twz_ptr_lea(obj, hdr->ctos), ptr, len, flags);
}

static void postprocess_char(struct object *obj, struct pty_hdr *hdr, unsigned char c)
{
	if(c == '\n' && (hdr->termios.c_oflag & ONLCR)) {
		char r = '\r';
		bstream_hdr_write(obj, twz_ptr_lea(obj, hdr->ctos), &r, 1, 0);
		bstream_hdr_write(obj, twz_ptr_lea(obj, hdr->ctos), &c, 1, 0);
	} else if(c == '\r' && (hdr->termios.c_oflag & OCRNL)) {
		char n = '\n';
		bstream_hdr_write(obj, twz_ptr_lea(obj, hdr->ctos), &n, 1, 0);
	} else {
		bstream_hdr_write(obj, twz_ptr_lea(obj, hdr->ctos), &c, 1, 0);
	}
}

static void echo_char(struct object *obj, struct pty_hdr *hdr, unsigned char c)
{
	if(hdr->termios.c_oflag & OPOST) {
		postprocess_char(obj, hdr, c);
	} else {
		bstream_hdr_write(obj, twz_ptr_lea(obj, hdr->ctos), &c, 1, 0);
	}
}

static void flush_input(struct object *obj, struct pty_hdr *hdr)
{
	size_t c = 0;
	ssize_t r;
	while(c < hdr->bufpos) {
		r =
		  bstream_hdr_write(obj, twz_ptr_lea(obj, hdr->stoc), hdr->buffer + c, hdr->bufpos - c, 0);
		/* TODO: what to do on error? */
		if(r < 0)
			break;
		c += r;
	}
	hdr->bufpos = 0;
}

static void process_input(struct object *obj, struct pty_hdr *hdr, int c)
{
	if(c == hdr->termios.c_cc[VERASE]) {
		if(hdr->bufpos > 0) {
			hdr->buffer[--hdr->bufpos] = 0;
			if(hdr->termios.c_lflag & ECHO) {
				echo_char(obj, hdr, '\b');
				echo_char(obj, hdr, ' ');
				echo_char(obj, hdr, '\b');
			}
		}
	} else if(c == hdr->termios.c_cc[VEOF]) {
		if(hdr->bufpos > 0) {
			flush_input(obj, hdr);
		} else {
			/* TODO: send EOF through by waking up and preventing reading */
		}
		/* TODO: I think these two below need to be done for _all_ input processing, not just in
		 * canonical mode */
	} else if(c == '\n' && (hdr->termios.c_iflag & INLCR)) {
		process_input(obj, hdr, '\r');
	} else if(c == '\r' && (hdr->termios.c_iflag & ICRNL)) {
		process_input(obj, hdr, '\n');
	} else {
		if(c == 27) /* ESC */
			c = '^';
		if(hdr->termios.c_lflag & ECHO)
			echo_char(obj, hdr, c);
		hdr->buffer[hdr->bufpos++] = c;
		if(c == '\n') {
			flush_input(obj, hdr);
		}
	}
}

ssize_t pty_write_server(struct object *obj, const void *ptr, size_t len, unsigned flags)
{
	struct pty_hdr *hdr = twz_obj_base(obj);
	if(hdr->termios.c_lflag & ICANON) {
		size_t count = 0;
		mutex_acquire(&hdr->buffer_lock);
		for(size_t i = 0; i < len; i++) {
			if(hdr->bufpos >= PTY_BUFFER_SZ)
				break;
			process_input(obj, hdr, ((char *)ptr)[i]);
		}
		mutex_release(&hdr->buffer_lock);
		return count;
	} else {
		size_t nl = len;
		if(hdr->termios.c_lflag & ECHO) {
			nl = bstream_hdr_write(obj, twz_ptr_lea(obj, hdr->ctos), ptr, len, flags);
		}
		return bstream_hdr_write(obj, twz_ptr_lea(obj, hdr->stoc), ptr, nl, flags);
	}
}

ssize_t pty_read_client(struct object *obj, void *ptr, size_t len, unsigned flags)
{
	struct pty_client_hdr *hdr = twz_obj_base(obj);
	struct pty_hdr *sh = twz_ptr_lea(obj, hdr->server);
	struct object sh_obj = TWZ_OBJECT_FROM_PTR(sh);
	return bstream_hdr_read(&sh_obj, twz_ptr_lea(&sh_obj, sh->stoc), ptr, len, flags);
}

ssize_t pty_write_client(struct object *obj, const void *ptr, size_t len, unsigned flags)
{
	struct pty_client_hdr *hdr = twz_obj_base(obj);
	struct pty_hdr *sh = twz_ptr_lea(obj, hdr->server);
	struct object sh_obj = TWZ_OBJECT_FROM_PTR(sh);
	if(sh->termios.c_oflag & OPOST) {
		for(size_t i = 0; i < len; i++) {
			postprocess_char(&sh_obj, sh, ((char *)ptr)[i]);
		}
		return len;
	} else {
		return bstream_hdr_write(&sh_obj, twz_ptr_lea(&sh_obj, sh->ctos), ptr, len, flags);
	}
}

int pty_obj_init_server(struct object *obj, struct pty_hdr *hdr)
{
	hdr->stoc = (struct bstream_hdr *)((char *)twz_ptr_local(hdr) + sizeof(struct pty_hdr));
	hdr->ctos = (struct bstream_hdr *)((char *)twz_ptr_local(hdr) + sizeof(struct pty_hdr)
	                                   + bstream_hdr_size(PTY_NBITS));

	int r;
	if((r = twz_object_addext(obj, TWZIO_METAEXT_TAG, &hdr->io)))
		return r;
	bstream_obj_init(obj, twz_ptr_lea(obj, hdr->stoc), PTY_NBITS);
	bstream_obj_init(obj, twz_ptr_lea(obj, hdr->ctos), PTY_NBITS);
	mutex_init(&hdr->buffer_lock);
	hdr->bufpos = 0;

	objid_t id;
	r = twz_name_resolve(NULL, PTY_CTRL_OBJ, NULL, 0, &id);
	if(r)
		return r;
	r = twz_ptr_make(
	  obj, id, TWZ_GATE_CALL(NULL, PTY_GATE_READ_SERVER), FE_READ | FE_EXEC, &hdr->io.read);
	if(r)
		return r;
	r = twz_ptr_make(
	  obj, id, TWZ_GATE_CALL(NULL, PTY_GATE_WRITE_SERVER), FE_READ | FE_EXEC, &hdr->io.write);
	if(r)
		return r;
	r = twz_ptr_make(
	  obj, id, TWZ_GATE_CALL(NULL, PTY_GATE_IOCTL_SERVER), FE_READ | FE_EXEC, &hdr->io.ioctl);
	if(r)
		return r;

	return 0;
}

int pty_obj_init_client(struct object *obj, struct pty_client_hdr *hdr, struct pty_hdr *sh)
{
	twz_ptr_store(obj, sh, FE_READ | FE_WRITE, &hdr->server);

	int r;
	if((r = twz_object_addext(obj, TWZIO_METAEXT_TAG, &hdr->io)))
		return r;
	objid_t id;
	r = twz_name_resolve(NULL, PTY_CTRL_OBJ, NULL, 0, &id);
	if(r)
		return r;
	r = twz_ptr_make(
	  obj, id, TWZ_GATE_CALL(NULL, PTY_GATE_READ_CLIENT), FE_READ | FE_EXEC, &hdr->io.read);
	if(r)
		return r;
	r = twz_ptr_make(
	  obj, id, TWZ_GATE_CALL(NULL, PTY_GATE_WRITE_CLIENT), FE_READ | FE_EXEC, &hdr->io.write);
	if(r)
		return r;
	r = twz_ptr_make(
	  obj, id, TWZ_GATE_CALL(NULL, PTY_GATE_IOCTL_CLIENT), FE_READ | FE_EXEC, &hdr->io.ioctl);
	if(r)
		return r;

	return 0;
}
