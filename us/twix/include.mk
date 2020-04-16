TWIX_SRC=$(addprefix us/twix/,syscall.c rw.c fd.c linux.c file.c thread.c access.c process.c time.c dir.c)
TWIX_OBJ=$(addprefix $(BUILDDIR)/,$(TWIX_SRC:.c=.o))

TWIXCFLAGS=-fsanitize=undefined -g

$(BUILDDIR)/us/twix/libtwix.a: $(TWIX_OBJ)
	@echo "[AR]      $@"
	#@ar rcs $(BUILDDIR)/us/twix/libtwix.a $(TWIX_OBJ)
	@mkdir -p $(dir $@)/tmp
	@echo "[ARx]     libubsan.a"
	@cd $(dir $@)/tmp && ar x $$($(TWZCC) -print-file-name=libubsan.a)
	@echo "[AR]      $@"
	@ar rcs $(BUILDDIR)/us/twix/libtwix.a $(TWIX_OBJ) $(BUILDDIR)/us/twix/tmp/*.o

$(BUILDDIR)/us/twix/libtwix.so: $(TWIX_OBJ)
	@echo "[LD]      $@"
	$(TWZCC) -o $@ -shared $(TWIX_OBJ)

$(BUILDDIR)/us/twix/%.o: us/twix/%.c $(MUSL_HDRS)
	@echo "[CC]      $@"
	@mkdir -p $(BUILDDIR)/us/twix
	$(TWZCC) $(TWZCFLAGS) $(TWIXCFLAGS) -c -o $@ $< -MD -fPIC

-include $(TWIX_OBJ:.o=.d)

