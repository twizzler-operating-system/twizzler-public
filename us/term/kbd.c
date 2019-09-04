#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#include <twz/debug.h>

#define KEY_CTRL_A 1
#define KEY_CTRL_B 2
#define KEY_CTRL_C 3
#define KEY_CTRL_D 4
#define KEY_CTRL_E 5
#define KEY_CTRL_F 6
#define KEY_CTRL_G 7
#define KEY_CTRL_H 8
#define KEY_CTRL_I 9
#define KEY_CTRL_J 10
#define KEY_CTRL_K 11
#define KEY_CTRL_L 12
#define KEY_CTRL_M 13
#define KEY_CTRL_N 14
#define KEY_CTRL_O 15
#define KEY_CTRL_P 16
#define KEY_CTRL_Q 17
#define KEY_CTRL_R 18
#define KEY_CTRL_S 19
#define KEY_CTRL_T 20
#define KEY_CTRL_U 21
#define KEY_CTRL_V 22
#define KEY_CTRL_W 23
#define KEY_CTRL_X 24
#define KEY_CTRL_Y 25
#define KEY_CTRL_Z 26
#define KEY_ESCAPE 27
#define KEY_ARROW_UP 257
#define KEY_ARROW_DOWN 258
#define KEY_ARROW_RIGHT 259
#define KEY_ARROW_LEFT 260

#define KEY_CTRL_ARROW_UP 261
#define KEY_CTRL_ARROW_DOWN 262
#define KEY_CTRL_ARROW_RIGHT 263
#define KEY_CTRL_ARROW_LEFT 264

#define KEY_SHIFT_ARROW_UP 265
#define KEY_SHIFT_ARROW_DOWN 266
#define KEY_SHIFT_ARROW_RIGHT 267
#define KEY_SHIFT_ARROW_LEFT 268

#define KEY_LEFT_CTRL 1001
#define KEY_LEFT_SHIFT 1002
#define KEY_LEFT_ALT 1003
#define KEY_LEFT_SUPER 1004

#define KEY_RIGHT_CTRL 1011
#define KEY_RIGHT_SHIFT 1012
#define KEY_RIGHT_ALT 1013
#define KEY_RIGHT_SUPER 1014

#define KEY_F1 2001
#define KEY_F2 2002
#define KEY_F3 2003
#define KEY_F4 2004
#define KEY_F5 2005
#define KEY_F6 2006
#define KEY_F7 2007
#define KEY_F8 2008
#define KEY_F9 2009
#define KEY_F10 2010
#define KEY_F11 2011
#define KEY_F12 2012

#define KEY_PAGE_DOWN 2013
#define KEY_PAGE_UP 2014

#define KEY_HOME 2015
#define KEY_END 2016
#define KEY_DEL 2017
#define KEY_INSERT 2018

#define KEY_BACKTAB 2019

#define KEY_SCANCODE_F1 0x3b
#define KEY_SCANCODE_F2 0x3c
#define KEY_SCANCODE_F3 0x3d
#define KEY_SCANCODE_F4 0x3e
#define KEY_SCANCODE_F5 0x3f
#define KEY_SCANCODE_F6 0x40
#define KEY_SCANCODE_F7 0x41
#define KEY_SCANCODE_F8 0x42
#define KEY_SCANCODE_F9 0x43
#define KEY_SCANCODE_F10 0x44
#define KEY_SCANCODE_F11 0x57
#define KEY_SCANCODE_F12 0x58

#define KEY_MOD_LEFT_CTRL 0x01
#define KEY_MOD_LEFT_SHIFT 0x02
#define KEY_MOD_LEFT_ALT 0x04
#define KEY_MOD_LEFT_SUPER 0x08

#define KEY_MOD_RIGHT_CTRL 0x10
#define KEY_MOD_RIGHT_SHIFT 0x20
#define KEY_MOD_RIGHT_ALT 0x40
#define KEY_MOD_RIGHT_SUPER 0x80

unsigned int us_map_ctrl[128] = {
	[0x01] = KEY_ESCAPE,
	[0x02] = '1',
	[0x03] = '2',
	[0x04] = '3',
	[0x05] = '4',
	[0x06] = '5',
	[0x07] = '6',
	[0x08] = '7',
	[0x09] = '8',
	[0x0a] = '9',
	[0x0b] = '0',
	[0x0c] = '-',
	[0x0d] = '=',
	[0x0e] = '\b',
	[0x0f] = '\t',
	[0x10] = KEY_CTRL_Q,
	[0x11] = KEY_CTRL_W,
	[0x12] = KEY_CTRL_E,
	[0x13] = KEY_CTRL_R,
	[0x14] = KEY_CTRL_T,
	[0x15] = KEY_CTRL_Y,
	[0x16] = KEY_CTRL_U,
	[0x17] = KEY_CTRL_I,
	[0x18] = KEY_CTRL_O,
	[0x19] = KEY_CTRL_P,
	[0x1a] = '[',
	[0x1b] = ']',
	[0x1c] = '\n',

	[0x1e] = KEY_CTRL_A,
	[0x1f] = KEY_CTRL_S,
	[0x20] = KEY_CTRL_D,
	[0x21] = KEY_CTRL_F,
	[0x22] = KEY_CTRL_G,
	[0x23] = KEY_CTRL_H,
	[0x24] = KEY_CTRL_J,
	[0x25] = KEY_CTRL_K,
	[0x26] = KEY_CTRL_L,
	[0x27] = ';',
	[0x28] = '\'',
	[0x29] = '`',

	[0x2b] = '\\',
	[0x2c] = KEY_CTRL_Z,
	[0x2d] = KEY_CTRL_X,
	[0x2e] = KEY_CTRL_C,
	[0x2f] = KEY_CTRL_V,
	[0x30] = KEY_CTRL_B,
	[0x31] = KEY_CTRL_N,
	[0x32] = KEY_CTRL_M,
	[0x33] = ',',
	[0x34] = '.',
	[0x35] = '/',

	[0x39] = ' ',

	[0x3b] = KEY_F1,
	[0x3c] = KEY_F2,
	[0x3d] = KEY_F3,
	[0x3e] = KEY_F4,
	[0x3f] = KEY_F5,
	[0x40] = KEY_F6,
	[0x41] = KEY_F7,
	[0x42] = KEY_F8,
	[0x43] = KEY_F9,
	[0x44] = KEY_F10,
	[0x52] = KEY_INSERT,
	[0x47] = KEY_HOME,
	[0x53] = KEY_DEL,
	[0x4f] = KEY_END,

	[0x49] = KEY_PAGE_UP,
	[0x51] = KEY_PAGE_DOWN,

	[0x48] = KEY_CTRL_ARROW_UP,
	[0x50] = KEY_CTRL_ARROW_DOWN,
	[0x4b] = KEY_CTRL_ARROW_LEFT,
	[0x4d] = KEY_CTRL_ARROW_RIGHT,
};

unsigned int us_map[128] = {
	[0x01] = KEY_ESCAPE,
	[0x02] = '1',
	[0x03] = '2',
	[0x04] = '3',
	[0x05] = '4',
	[0x06] = '5',
	[0x07] = '6',
	[0x08] = '7',
	[0x09] = '8',
	[0x0a] = '9',
	[0x0b] = '0',
	[0x0c] = '-',
	[0x0d] = '=',
	[0x0e] = '\b',
	[0x0f] = '\t',
	[0x10] = 'q',
	[0x11] = 'w',
	[0x12] = 'e',
	[0x13] = 'r',
	[0x14] = 't',
	[0x15] = 'y',
	[0x16] = 'u',
	[0x17] = 'i',
	[0x18] = 'o',
	[0x19] = 'p',
	[0x1a] = '[',
	[0x1b] = ']',
	[0x1c] = '\n',

	[0x1e] = 'a',
	[0x1f] = 's',
	[0x20] = 'd',
	[0x21] = 'f',
	[0x22] = 'g',
	[0x23] = 'h',
	[0x24] = 'j',
	[0x25] = 'k',
	[0x26] = 'l',
	[0x27] = ';',
	[0x28] = '\'',
	[0x29] = '`',

	[0x2b] = '\\',
	[0x2c] = 'z',
	[0x2d] = 'x',
	[0x2e] = 'c',
	[0x2f] = 'v',
	[0x30] = 'b',
	[0x31] = 'n',
	[0x32] = 'm',
	[0x33] = ',',
	[0x34] = '.',
	[0x35] = '/',

	[0x39] = ' ',

	[0x3b] = KEY_F1,
	[0x3c] = KEY_F2,
	[0x3d] = KEY_F3,
	[0x3e] = KEY_F4,
	[0x3f] = KEY_F5,
	[0x40] = KEY_F6,
	[0x41] = KEY_F7,
	[0x42] = KEY_F8,
	[0x43] = KEY_F9,
	[0x44] = KEY_F10,

	[0x49] = KEY_PAGE_UP,
	[0x51] = KEY_PAGE_DOWN,

	[0x52] = KEY_INSERT,
	[0x47] = KEY_HOME,
	[0x53] = KEY_DEL,
	[0x4f] = KEY_END,

	[0x48] = KEY_ARROW_UP,
	[0x50] = KEY_ARROW_DOWN,
	[0x4b] = KEY_ARROW_LEFT,
	[0x4d] = KEY_ARROW_RIGHT,

};

unsigned int us_map_shift[128] = {
	[0x01] = KEY_ESCAPE,
	[0x02] = '!',
	[0x03] = '@',
	[0x04] = '#',
	[0x05] = '$',
	[0x06] = '%',
	[0x07] = '^',
	[0x08] = '&',
	[0x09] = '*',
	[0x0a] = '(',
	[0x0b] = ')',
	[0x0c] = '_',
	[0x0d] = '+',
	[0x0e] = '\b',
	[0x0f] = KEY_BACKTAB,
	[0x10] = 'q',
	[0x11] = 'w',
	[0x12] = 'e',
	[0x13] = 'r',
	[0x14] = 't',
	[0x15] = 'y',
	[0x16] = 'u',
	[0x17] = 'i',
	[0x18] = 'o',
	[0x19] = 'p',
	[0x1a] = '{',
	[0x1b] = '}',
	[0x1c] = '\n',

	[0x1e] = 'a',
	[0x1f] = 's',
	[0x20] = 'd',
	[0x21] = 'f',
	[0x22] = 'g',
	[0x23] = 'h',
	[0x24] = 'j',
	[0x25] = 'k',
	[0x26] = 'l',
	[0x27] = ':',
	[0x28] = '"',
	[0x29] = '~',

	[0x2b] = '|',
	[0x2c] = 'z',
	[0x2d] = 'x',
	[0x2e] = 'c',
	[0x2f] = 'v',
	[0x30] = 'b',
	[0x31] = 'n',
	[0x32] = 'm',
	[0x33] = '<',
	[0x34] = '>',
	[0x35] = '?',

	[0x3b] = KEY_F1,
	[0x3c] = KEY_F2,
	[0x3d] = KEY_F3,
	[0x3e] = KEY_F4,
	[0x3f] = KEY_F5,
	[0x40] = KEY_F6,
	[0x41] = KEY_F7,
	[0x42] = KEY_F8,
	[0x43] = KEY_F9,
	[0x44] = KEY_F10,
	[0x52] = KEY_INSERT,
	[0x47] = KEY_HOME,
	[0x53] = KEY_DEL,
	[0x4f] = KEY_END,

	[0x39] = ' ',
	[0x49] = KEY_PAGE_UP,
	[0x51] = KEY_PAGE_DOWN,

	[0x48] = KEY_SHIFT_ARROW_UP,
	[0x50] = KEY_SHIFT_ARROW_DOWN,
	[0x4b] = KEY_SHIFT_ARROW_LEFT,
	[0x4d] = KEY_SHIFT_ARROW_RIGHT,

};

#include <twz/io.h>
#include <twz/obj.h>

void curfb_putc(int c);
void sendkey(struct object *out_obj, unsigned char key)
{
	twzio_write(out_obj, &key, 1, 0, 0);
	curfb_putc(key);
	if(key == '\n')
		curfb_putc('\r');
}
#include <string.h>
void sendescstr(struct object *out_obj, char *str)
{
	char buf[16];
	memset(buf, 0, sizeof(buf));
	buf[0] = 27;
	strncpy(&buf[1], str, 14);
	twzio_write(out_obj, buf, strlen(buf), 0, 0);
	curfb_putc('^');
	for(size_t i = 0; i < strlen(buf); i++)
		curfb_putc(str[i]);
	// bstream_write(&sobj, "^", 1, 0);
	// bstream_write(&sobj, str, strlen(str), 0);
}

bool ctrl = false, shift = false, alt = false, capslock = false;

void special_key(struct object *out_obj, unsigned int key)
{
	if(key == KEY_PAGE_DOWN && shift) {
		// scroll_down(current_pty, 20);
	} else if(key == KEY_PAGE_UP && shift) {
		//	scroll_up(current_pty, 20);
	} else if(key == KEY_SHIFT_ARROW_DOWN) {
		//	scroll_down(current_pty, 1);
	} else if(key == KEY_SHIFT_ARROW_UP) {
		//	scroll_up(current_pty, 1);
	} else if(key == KEY_ARROW_UP) {
		sendescstr(out_obj, "[A");
	} else if(key == KEY_ARROW_DOWN) {
		sendescstr(out_obj, "[B");
	} else if(key == KEY_ARROW_LEFT) {
		sendescstr(out_obj, "[D");
	} else if(key == KEY_ARROW_RIGHT) {
		sendescstr(out_obj, "[C");
	} else if(key >= KEY_F1 && key <= KEY_F9 && ctrl) {
		//	int console = key - KEY_F1;
		//	switch_console(console);
	} else if(key >= KEY_F1 && key < KEY_F10) {
		char str[5];
		str[0] = '[';
		str[1] = '1' + (key - KEY_F1);
		str[2] = 'F';
		str[3] = 0;
		sendescstr(out_obj, str);
	} else if(key == KEY_DEL) {
		if(shift)
			sendescstr(out_obj, "[^4~");
		else
			sendescstr(out_obj, "[4~");
	} else if(key == KEY_INSERT) {
		if(shift)
			sendescstr(out_obj, "[^1~");
		else
			sendescstr(out_obj, "[1~");
	} else if(key == KEY_HOME) {
		if(shift)
			sendescstr(out_obj, "[^2~");
		else
			sendescstr(out_obj, "[2~");
	} else if(key == KEY_END) {
		if(shift)
			sendescstr(out_obj, "[^5~");
		else
			sendescstr(out_obj, "[5~");
	} else if(key == KEY_PAGE_DOWN) {
		sendescstr(out_obj, "[6~");
	} else if(key == KEY_PAGE_UP) {
		sendescstr(out_obj, "[3~");
	} else if(key == KEY_BACKTAB) {
		sendescstr(out_obj, "[Z");
	}
}

void keyboard_state_machine(struct object *out_obj, unsigned char scancode)
{
	/* this will process the incoming scancodes, translate them to
	 * a character stream, and write them to the masterfd of the
	 * current pty. */
	bool release = false;
	if(!scancode)
		return;
	/* first, take care of protocol codes */
	switch(scancode) {
		case 0xee:
		case 0xf1:
		case 0xfa:
			return;
		case 0xfc:
		case 0xfd:
		case 0xfe:
		case 0xff:
			debug_printf("internal keyboard error: %x\n", scancode);
			return;
	}
	/* TODO: actually handle escaped codes */
	if(scancode == 0xe0)
		return;
	if(scancode & 0x80) {
		release = true;
		scancode -= 0x80;
	}
	unsigned int key = 0;
	switch(scancode) {
		case 0x1d:
			ctrl = !release;
			break;
		case 0x2a:
			shift = !release;
			break;
		case 0x38:
			alt = !release;
			break;
		case 0x3a:
			capslock = !capslock;
			break;
		default:
			if(shift)
				key = us_map_shift[scancode];
			else if(ctrl)
				key = us_map_ctrl[scancode];
			else
				key = us_map[scancode];
			if(shift ^ capslock && key < 256)
				key = toupper(key);
			if(key == 0)
				debug_printf("unknown scancode: %x (%d %d %d)\n", scancode, shift, ctrl, alt);
			else if(!release && key < 256) {
				sendkey(out_obj, (unsigned char)key);
			} else if(!release) {
				special_key(out_obj, key);
			}
	}
}

void process_keyboard(struct object *out_obj, char *buf, size_t len)
{
	while(len--) {
		unsigned char c = *buf++;
		keyboard_state_machine(out_obj, c);
	}
}
