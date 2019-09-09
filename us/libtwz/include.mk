LIBTWZ_SRC=$(addprefix us/libtwz/,object.c fault.c thread.c view.c name.c oa.c btree.c event.c mutex.c bstream.c io.c exec.c)

LIBTWZ_OBJ=$(addprefix $(BUILDDIR)/,$(LIBTWZ_SRC:.c=.o))

$(BUILDDIR)/us/libtwz/%.o: us/libtwz/%.c $(MUSL_HDRS)
	@mkdir -p $(dir $@)
	@echo "[CC] $@"
	@$(TWZCC) $(TWZCFLAGS) -c -o $@ -MD -fPIC $<

$(BUILDDIR)/us/libtwz/libtwz.a: $(LIBTWZ_OBJ)
	@mkdir -p $(dir $@)
	@echo "[AR]  $@"
	@ar rcs $(BUILDDIR)/us/libtwz/libtwz.a $(LIBTWZ_OBJ)

# TODO: use the TWZCC compiler
$(BUILDDIR)/us/libtwz/libtwz.so: $(LIBTWZ_OBJ) $(BUILDDIR)/us/twix/libtwix.a
	@mkdir -p $(dir $@)
	@echo "[LD]  $@"
	@$(TOOLCHAIN_PREFIX)gcc -o $(BUILDDIR)/us/libtwz/libtwz.so -fpic -shared $(LIBTWZ_OBJ) $(MUSL_STATIC_LIBC) $(BUILDDIR)/us/twix/libtwix.a -T us/elf.ld -Wl,-z,max-page-size=0x1000
	#@$(TWZCC) -o $(BUILDDIR)/us/libtwz/libtwz.so -fpie -shared $(LIBTWZ_OBJ) -nostdlib

-include $(LIBTWZ_OBJ:.o=.d)
