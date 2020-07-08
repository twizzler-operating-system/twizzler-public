
E1000_SRCS=$(addprefix us/drivers/e1000/,e1000.cpp)
E1000_OBJS=$(addprefix $(BUILDDIR)/,$(E1000_SRCS:.cpp=.o))

#E1000_LIBS=-Wl,--whole-archive -lbacktrace -Wl,--no-whole-archive
#E1000_CFLAGS=-fsanitize=undefined

$(BUILDDIR)/us/sysroot/usr/bin/e1000: $(E1000_OBJS) $(SYSROOT_READY) $(SYSLIBS) $(UTILS)
	@mkdir -p $(dir $@)
	@echo "[LD]      $@"
	@$(TWZCXX) $(TWZLDFLAGS) -g -o $@ -MD $< $(E1000_LIBS) 

$(BUILDDIR)/us/drivers/e1000/%.o: us/drivers/e1000/%.cpp $(MUSL_HDRS)
	@mkdir -p $(dir $@)
	@echo "[CC]      $@"
	@$(TWZCXX) $(TWZCFLAGS) $(E1000_CFLAGS) -o $@ -c -MD $<

SYSROOT_FILES+=$(BUILDDIR)/us/sysroot/usr/bin/e1000

