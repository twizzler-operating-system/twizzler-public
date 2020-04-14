LIBTWZ_SRC=$(addprefix us/libtwz/,object.c fault.c thread.c view.c name.c oa.c btree.c event.c mutex.c bstream.c io.c exec.c pty.c hier.c kso.c libtwz.c)

LIBTWZ_OBJ=$(addprefix $(BUILDDIR)/,$(LIBTWZ_SRC:.c=.o))

LIBTWZCFLAGS=-fsanitize=undefined

$(BUILDDIR)/us/libtwz/%.o: us/libtwz/%.c $(MUSL_HDRS)
	@mkdir -p $(dir $@)
	@echo "[CC]      $@"
	@$(TWZCC) $(TWZCFLAGS) $(LIBTWZCFLAGS) -fno-omit-frame-pointer -g -Ius/libtwz/include -include us/libtwz/include/libtwz.h -c -o $@ -MD -fPIC $<

$(BUILDDIR)/us/libtwz/libtwz.a: $(LIBTWZ_OBJ)
	@mkdir -p $(dir $@)
	#@ar rcs $(BUILDDIR)/us/libtwz/libtwz.a $(LIBTWZ_OBJ)
	@mkdir -p $(dir $@)/tmp
	@mkdir -p $(dir $@)/tmp2
	@echo "[ARx]     libubsan.a"
	@cd $(dir $@)/tmp && ar x $$($(TWZCC) -print-file-name=libubsan.a)
	#@echo "[ARx]     libbacktrace.a"
	#@cd $(dir $@)/tmp2 && ar x $$($(TWZCC) -print-file-name=libbacktrace.a)
	@echo "[AR]      $@"
	@ar rcs $(BUILDDIR)/us/libtwz/libtwz.a $(LIBTWZ_OBJ) $(BUILDDIR)/us/libtwz/tmp/*.o

$(BUILDDIR)/us/libtwz/libtwz.so: $(LIBTWZ_OBJ) $(BUILDDIR)/us/twix/libtwix.a
	@mkdir -p $(dir $@)
	@echo "[LD]      $@"
	@$(TWZCC) -o $(BUILDDIR)/us/libtwz/libtwz.so -shared $(LIBTWZ_OBJ) -nostdlib

-include $(LIBTWZ_OBJ:.o=.d)
