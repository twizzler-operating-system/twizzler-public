TWIX_SRC=$(addprefix us/twix/,syscall.c rw.c fd.c linux.c file.c thread.c access.c process.c time.c dir.c rand.c)
TWIX_OBJ=$(addprefix $(BUILDDIR)/,$(TWIX_SRC:.c=.o))

ifneq (,$(wildcard $(shell $(TWZCC) -print-file-name=libubsan.a)))
TWIXCFLAGS=-fsanitize=undefined -g
endif

$(BUILDDIR)/us/twix/libtwix.a: $(TWIX_OBJ)
	@echo "[AR]      $@"
	#@ar rcs $(BUILDDIR)/us/twix/libtwix.a $(TWIX_OBJ)
	@mkdir -p $(dir $@)/tmp
	@if [ -f $$($(TWZCC) -print-file-name=libubsan.a) ]; then \
		echo "[ARx]     libubsan.a";\
		(cd $(dir $@)/tmp;\
		ar x $$($(TWZCC) -print-file-name=libubsan.a););\
		echo "[AR]      $@";\
		ar rcs $(BUILDDIR)/us/twix/libtwix.a $(TWIX_OBJ) $(BUILDDIR)/us/twix/tmp/*.o;\
	else\
		echo "[AR]      $@";\
		ar rcs $(BUILDDIR)/us/twix/libtwix.a $(TWIX_OBJ);\
	fi


$(BUILDDIR)/us/twix/libtwix.so: $(TWIX_OBJ)
	@echo "[LD]      $@"
	$(TWZCC) -o $@ -shared $(TWIX_OBJ)

$(BUILDDIR)/us/twix/%.o: us/twix/%.c $(MUSL_HDRS)
	@echo "[CC]      $@"
	@mkdir -p $(BUILDDIR)/us/twix
	$(TWZCC) $(TWZCFLAGS) $(TWIXCFLAGS) -c -o $@ $< -MD -fPIC

-include $(TWIX_OBJ:.o=.d)

