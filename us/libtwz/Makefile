SRC=notify.c bstream.c mutex.c twzio.c viewcall.c twzlog.c name.c corecall.c debug.c object.c blake2.c secctx.c thread.c
OBJ=$(SRC:.c=.o)
DEP=$(SRC:.c=.d)

CFLAGS=-Wall -Wextra -std=gnu11 -Os -g -I../include -mcmodel=large -fpie #-fno-plt -fvisibility=internal
LDFLAGS=-Wl,-z,max-page-size=0x1000 -Telf.ld -pie # -fno-plt -fvisibility=internal
CC=$(

all: libtwz.so libtwz.a

libtwz.a: $(OBJ) Makefile
	x86_64-elf-linux-musl-ar rcs libtwz.a $(OBJ)

libtwz.so: $(OBJ) Makefile
	$(CC) $(LDFLAGS) -shared -o libtwz.so $(OBJ)
	../postelf/postelf libtwz.so

.c.o: Makefile
	$(CC) $< -c -o $@ $(CFLAGS) -MD

clean:
	-rm $(OBJ) $(DEP) libtwz.so libtwz.so.0 libtwz.so.1 libtwz.a *.meta

-include $(DEP)

