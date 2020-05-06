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

int pty_ioctl_server(twzobj *obj, int request, long arg)
{
	int r = 0;
	struct pty_hdr *hdr = twz_object_base(obj);
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

int pty_ioctl_client(twzobj *obj, int request, long arg)
{
	struct pty_client_hdr *hdr = twz_object_base(obj);
	struct pty_hdr *sh = twz_object_lea(obj, hdr->server);
	twzobj sh_obj = twz_object_from_ptr(sh);
	return pty_ioctl_server(&sh_obj, request, arg);
}

ssize_t pty_read_server(twzobj *obj, void *ptr, size_t len, unsigned flags)
{
	struct pty_hdr *hdr = twz_object_base(obj);
	return bstream_hdr_read(obj, twz_object_lea(obj, hdr->ctos), ptr, len, flags);
}

static void postprocess_char(twzobj *obj, struct pty_hdr *hdr, unsigned char c)
{
	if(c == '\n' && (hdr->termios.c_oflag & ONLCR)) {
		char r = '\r';
		bstream_hdr_write(obj, twz_object_lea(obj, hdr->ctos), &r, 1, 0);
		bstream_hdr_write(obj, twz_object_lea(obj, hdr->ctos), &c, 1, 0);
	} else if(c == '\r' && (hdr->termios.c_oflag & OCRNL)) {
		char n = '\n';
		bstream_hdr_write(obj, twz_object_lea(obj, hdr->ctos), &n, 1, 0);
	} else {
		bstream_hdr_write(obj, twz_object_lea(obj, hdr->ctos), &c, 1, 0);
	}
}

static void echo_char(twzobj *obj, struct pty_hdr *hdr, unsigned char c)
{
	if(hdr->termios.c_oflag & OPOST) {
		postprocess_char(obj, hdr, c);
	} else {
		bstream_hdr_write(obj, twz_object_lea(obj, hdr->ctos), &c, 1, 0);
	}
}

static void flush_input(twzobj *obj, struct pty_hdr *hdr)
{
	size_t c = 0;
	ssize_t r;
	while(c < hdr->bufpos) {
		r = bstream_hdr_write(
		  obj, twz_object_lea(obj, hdr->stoc), hdr->buffer + c, hdr->bufpos - c, 0);
		/* TODO: what to do on error? */
		if(r < 0)
			break;
		c += r;
	}
	hdr->bufpos = 0;
}

static void process_input(twzobj *obj, struct pty_hdr *hdr, int c)
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

ssize_t pty_write_server(twzobj *obj, const void *ptr, size_t len, unsigned flags)
{
	struct pty_hdr *hdr = twz_object_base(obj);
	// debug_printf("ptysw: " IDFMT "\n", IDPR(twz_object_guid(obj)));
	if(hdr->termios.c_lflag & ICANON) {
		size_t count = 0;
		mutex_acquire(&hdr->buffer_lock);
		for(size_t i = 0; i < len; i++, count++) {
			if(hdr->bufpos >= PTY_BUFFER_SZ)
				break;
			process_input(obj, hdr, ((char *)ptr)[i]);
		}
		mutex_release(&hdr->buffer_lock);
		return count;
	} else {
		size_t nl = len;
		if(hdr->termios.c_lflag & ECHO) {
			nl = bstream_hdr_write(obj, twz_object_lea(obj, hdr->ctos), ptr, len, flags);
		}
		return bstream_hdr_write(obj, twz_object_lea(obj, hdr->stoc), ptr, nl, flags);
	}
}

ssize_t pty_read_client(twzobj *obj, void *ptr, size_t len, unsigned flags)
{
	struct pty_client_hdr *hdr = twz_object_base(obj);
	struct pty_hdr *sh = twz_object_lea(obj, hdr->server);
	twzobj sh_obj = twz_object_from_ptr(sh);
	return bstream_hdr_read(&sh_obj, twz_object_lea(&sh_obj, sh->stoc), ptr, len, flags);
}

ssize_t pty_write_client(twzobj *obj, const void *ptr, size_t len, unsigned flags)
{
	struct pty_client_hdr *hdr = twz_object_base(obj);
	struct pty_hdr *sh = twz_object_lea(obj, hdr->server);
	twzobj sh_obj = twz_object_from_ptr(sh);
	if(sh->termios.c_oflag & OPOST) {
		for(size_t i = 0; i < len; i++) {
			postprocess_char(&sh_obj, sh, ((char *)ptr)[i]);
		}
		return len;
	} else {
		return bstream_hdr_write(&sh_obj, twz_object_lea(&sh_obj, sh->ctos), ptr, len, flags);
	}
}

int pty_poll_client(twzobj *obj, uint64_t type, struct event *event)
{
	struct pty_client_hdr *hdr = twz_object_base(obj);
	struct pty_hdr *sh = twz_object_lea(obj, hdr->server);

	struct bstream_hdr *bh;
	twzobj to = twz_object_from_ptr(sh);
	// debug_printf("pty poll client " IDFMT " :: " IDFMT "\n",
	// IDPR(twz_object_guid(obj)),
	// IDPR(twz_object_guid(&to)));
	if(type == TWZIO_EVENT_READ) {
		bh = twz_object_lea(&to, sh->stoc);
	} else if(type == TWZIO_EVENT_WRITE) {
		bh = twz_object_lea(&to, sh->ctos);
	} else {
		return -ENOTSUP;
	}

	return bstream_hdr_poll(&to, bh, type, event);
}

int pty_poll_server(twzobj *obj, uint64_t type, struct event *event)
{
	struct pty_hdr *hdr = twz_object_base(obj);

	struct bstream_hdr *bh;
	twzobj *to = obj;
	if(type & TWZIO_EVENT_READ) {
		bh = twz_object_lea(obj, hdr->ctos);
	} else if(type & TWZIO_EVENT_WRITE) {
		bh = twz_object_lea(obj, hdr->stoc);
	} else {
		return -ENOTSUP;
	}

	return bstream_hdr_poll(obj, bh, type, event);
}

int pty_obj_init_server(twzobj *obj, struct pty_hdr *hdr)
{
	hdr->stoc = (struct bstream_hdr *)((char *)twz_ptr_local(hdr) + sizeof(struct pty_hdr));
	hdr->ctos = (struct bstream_hdr *)((char *)twz_ptr_local(hdr) + sizeof(struct pty_hdr)
	                                   + bstream_hdr_size(PTY_NBITS));

	int r;
	if((r = twz_object_addext(obj, TWZIO_METAEXT_TAG, &hdr->io))) {
		goto cleanup;
	}
	if((r = bstream_obj_init(obj, twz_object_lea(obj, hdr->stoc), PTY_NBITS))) {
		goto cleanup;
	}
	if((r = bstream_obj_init(obj, twz_object_lea(obj, hdr->ctos), PTY_NBITS))) {
		goto cleanup;
	}
	mutex_init(&hdr->buffer_lock);
	hdr->bufpos = 0;

	twzobj co;
	r = twz_object_init_name(&co, PTY_CTRL_OBJ, FE_READ | FE_EXEC);
	if(r) {
		goto cleanup;
	}

	r = twz_ptr_store_guid(
	  obj, &hdr->io.read, &co, TWZ_GATE_CALL(NULL, PTY_GATE_READ_SERVER), FE_READ | FE_EXEC);
	if(r) {
		goto cleanup2;
	}
	r = twz_ptr_store_guid(
	  obj, &hdr->io.write, &co, TWZ_GATE_CALL(NULL, PTY_GATE_WRITE_SERVER), FE_READ | FE_EXEC);
	if(r) {
		goto cleanup2;
	}
	r = twz_ptr_store_guid(
	  obj, &hdr->io.ioctl, &co, TWZ_GATE_CALL(NULL, PTY_GATE_IOCTL_SERVER), FE_READ | FE_EXEC);
	if(r) {
		goto cleanup2;
	}
	r = twz_ptr_store_guid(
	  obj, &hdr->io.poll, &co, TWZ_GATE_CALL(NULL, PTY_GATE_POLL_SERVER), FE_READ | FE_EXEC);
	if(r) {
		goto cleanup2;
	}

	twz_object_release(&co);
	return 0;

cleanup2:
	twz_object_release(&co);
cleanup:
	twz_object_delext(obj, TWZIO_METAEXT_TAG, &hdr->io);
	return r;
}

int pty_obj_init_client(twzobj *obj, struct pty_client_hdr *hdr, struct pty_hdr *sh)
{
	int r;
	if((r = twz_ptr_store_guid(obj, &hdr->server, NULL, sh, FE_READ | FE_WRITE))) {
		goto cleanup;
	}

	if((r = twz_object_addext(obj, TWZIO_METAEXT_TAG, &hdr->io))) {
		goto cleanup;
	}

	twzobj co;
	r = twz_object_init_name(&co, PTY_CTRL_OBJ, FE_READ | FE_EXEC);
	if(r) {
		goto cleanup;
	}

	r = twz_ptr_store_guid(
	  obj, &hdr->io.read, &co, TWZ_GATE_CALL(NULL, PTY_GATE_READ_CLIENT), FE_READ | FE_EXEC);
	if(r) {
		goto cleanup2;
	}
	r = twz_ptr_store_guid(
	  obj, &hdr->io.write, &co, TWZ_GATE_CALL(NULL, PTY_GATE_WRITE_CLIENT), FE_READ | FE_EXEC);
	if(r) {
		goto cleanup2;
	}
	r = twz_ptr_store_guid(
	  obj, &hdr->io.ioctl, &co, TWZ_GATE_CALL(NULL, PTY_GATE_IOCTL_CLIENT), FE_READ | FE_EXEC);
	if(r) {
		goto cleanup2;
	}
	r = twz_ptr_store_guid(
	  obj, &hdr->io.poll, &co, TWZ_GATE_CALL(NULL, PTY_GATE_POLL_CLIENT), FE_READ | FE_EXEC);
	if(r) {
		goto cleanup2;
	}

	twz_object_release(&co);
	return 0;

cleanup2:
	twz_object_release(&co);
cleanup:
	twz_object_delext(obj, TWZIO_METAEXT_TAG, &hdr->io);
	return r;
}
