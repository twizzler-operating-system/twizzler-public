LIBTWZ_SRC=$(addprefix us/libtwz/,object.c fault.c thread.c view.c name.c oa.c btree.c event.c mutex.c bstream.c io.c)

LIBTWZ_OBJ=$(addprefix $(BUILDDIR)/,$(LIBTWZ_SRC:.c=.o))

$(BUILDDIR)/us/libtwz/libtwz.a: $(LIBTWZ_OBJ)
	@mkdir -p $(dir $@)
	@echo "[AR]  $@"
	@ar rcs $(BUILDDIR)/us/libtwz/libtwz.a $(LIBTWZ_OBJ)

$(BUILDDIR)/us/libtwz/libtwz.so: $(LIBTWZ_OBJ)
	@mkdir -p $(dir $@)
	@echo "[AR]  $@"
	@$(TWZCC) -T us/elf.ld -o $(BUILDDIR)/us/libtwz/libtwz.so -fpic -shared $(LIBTWZ_OBJ) $(MUSL_STATIC_LIBC) $(BUILDDIR)/us/twix/libtwix.a

-include $(LIBTWZ_OBJ:.o=.d)
