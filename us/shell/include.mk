shell_srcs=$(addprefix us/shell/,shell.c)

shell_objs=$(addprefix $(BUILDDIR)/,$(shell_srcs:.c=.o))
shell_deps=$(addprefix $(BUILDDIR)/,$(shell_srcs:.c=.d))

shell_all: $(BUILDDIR)/us/shell/shell

$(BUILDDIR)/us/shell/shell: $(shell_objs) $(US_LIBDEPS)
	@echo "[CLD] $@"
	@$(TWZCC) $(US_LDFLAGS) $(US_CFLAGS) -o $@ -nostdlib $(US_PRELINK) $< $(US_POSTLINK) -MD

-include $(shell_deps)

.PHONY: shell_all
