
PAGER_SRCS=$(addprefix us/bin/pager/,pager.cpp)
PAGER_OBJS=$(addprefix $(BUILDDIR)/,$(PAGER_SRCS:.cpp=.o))

#PAGER_LIBS=-Wl,--whole-archive -lbacktrace -Wl,--no-whole-archive
#PAGER_CFLAGS=-fsanitize=undefined

$(BUILDDIR)/us/sysroot/usr/bin/pager: $(PAGER_OBJS) $(SYSROOT_READY) $(SYSLIBS) $(UTILS)
	@mkdir -p $(dir $@)
	@echo "[LD]      $@"
	@$(TWZCXX) $(TWZLDFLAGS) -g -o $@ -MD $< $(PAGER_LIBS) 

$(BUILDDIR)/us/bin/pager/%.o: us/bin/pager/%.cpp $(MUSL_HDRS)
	@mkdir -p $(dir $@)
	@echo "[CC]      $@"
	@$(TWZCXX) $(TWZCFLAGS) $(PAGER_CFLAGS) -o $@ -c -MD $<

SYSROOT_FILES+=$(BUILDDIR)/us/sysroot/usr/bin/pager

