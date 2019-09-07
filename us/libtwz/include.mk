LIBTWZ_SRC=$(addprefix us/libtwz/,object.c fault.c thread.c view.c name.c oa.c btree.c event.c mutex.c bstream.c io.c exec.c)

LIBTWZ_OBJ=$(addprefix $(BUILDDIR)/,$(LIBTWZ_SRC:.c=.o))

$(BUILDDIR)/us/libtwz/%.o: us/libtwz/%.c
	@mkdir -p $(dir $@)
	@echo "[CC] $@"
	@$(TWZCC) $(TWZCFLAGS) -c -o $@ -MD -fPIC $<

$(BUILDDIR)/us/libtwz/libtwz.a: $(LIBTWZ_OBJ)
	@mkdir -p $(dir $@)
	@echo "[AR]  $@"
	@ar rcs $(BUILDDIR)/us/libtwz/libtwz.a $(LIBTWZ_OBJ)

$(BUILDDIR)/us/libtwz/libtwz.so: $(LIBTWZ_OBJ)
	@mkdir -p $(dir $@)
	@echo "[AR]  $@"
	@x86_64-pc-elf-gcc $(US_LDFLAGS) -o $(BUILDDIR)/us/libtwz/libtwz.so -fpic -shared $(LIBTWZ_OBJ) $(MUSL_STATIC_LIBC) $(BUILDDIR)/us/twix/libtwix.a

-include $(LIBTWZ_OBJ:.o=.d)
