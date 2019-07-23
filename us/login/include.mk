login_srcs=$(addprefix us/login/,login.c)

login_objs=$(addprefix $(BUILDDIR)/,$(login_srcs:.c=.o))
login_deps=$(addprefix $(BUILDDIR)/,$(login_srcs:.c=.d))

login_all: $(BUILDDIR)/us/login/login

$(BUILDDIR)/us/login/login: $(login_objs) $(US_LIBDEPS)
	@echo "[CLD] $@"
	@$(TWZCC) $(US_LDFLAGS) $(US_CFLAGS) -o $@ -nostdlib $(US_PRELINK) $< $(US_POSTLINK) -MD

-include $(login_deps)

.PHONY: login_all
