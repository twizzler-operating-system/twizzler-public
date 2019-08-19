nls_srcs=$(addprefix us/nls/,nls.c)

nls_objs=$(addprefix $(BUILDDIR)/,$(nls_srcs:.c=.o))
nls_deps=$(addprefix $(BUILDDIR)/,$(nls_srcs:.c=.d))

nls_all: $(BUILDDIR)/us/nls/nls

$(BUILDDIR)/us/nls/nls: $(nls_objs) $(US_LIBDEPS)
	@echo "[CLD] $@"
	@$(TWZCC) $(US_LDFLAGS) $(US_CFLAGS) -o $@ -nostdlib $(US_PRELINK) $< $(US_POSTLINK) -MD

-include $(nls_deps)

.PHONY: nls_all
