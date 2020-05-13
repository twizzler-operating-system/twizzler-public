PLAYGROUND_PROGS=example

PLAYGROUND_LIBS=-Wl,--whole-archive -lbacktrace -Wl,--no-whole-archive

PLAYGROUND_CFLAGS=-g -fno-omit-frame-pointer

$(BUILDDIR)/us/sysroot/usr/bin/%: $(BUILDDIR)/us/playground/%.o $(SYSROOT_READY) $(SYSLIBS) $(UTILS) $(ALL_EXTRAS)
	@echo "[LD]      $@"
	@$(TWZCC) $(TWZLDFLAGS) -g -o $@ -MD $< $(EXTRAS_$(notdir $@)) $(LIBS_$(notdir $@)) $(PLAYGROUND_LIBS)

$(BUILDDIR)/us/playground/%.o: us/playground/%.c $(MUSL_HDRS)
	@mkdir -p $(dir $@)
	@echo "[CC]      $@"
	@$(TWZCC) $(TWZCFLAGS) $(PLAYGROUND_CFLAGS) -o $@ $(CFLAGS_$(basename $(notdir $@))) -c -MD $<

SYSROOT_FILES+=$(addprefix $(BUILDDIR)/us/sysroot/usr/bin/,$(PLAYGROUND_PROGS))

