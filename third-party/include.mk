CFLAGS_LTM=-DMP_NO_DEV_URANDOM -DMP_NO_FILE
CFLAGS_LTC=-DLTM_DESC -DLTC_NO_FILE -DLTC_NO_ASM -DMP_NO_FILE

CFLAGS+=$(CFLAGS_LTM) $(CFLAGS_LTC)
CFLAGS_TP=$(CORE_CFLAGS) $(addprefix -I../../,$(INCLUDE_DIRS)) -I../include -D__KERNEL__

INCLUDE_DIRS+=$(BUILDDIR)/third-party/libtommath/include
INCLUDE_DIRS+=$(BUILDDIR)/third-party/libtomcrypt/include

KLIBS=$(BUILDDIR)/third-party/libtommath/lib/libtommath.a
KLIBS=$(BUILDDIR)/third-party/libtomcrypt/lib/libtomcrypt.a

$(BUILDDIR)/third-party/libtommath/lib/libtommath.a:
$(BUILDDIR)/third-party/libtommath/include/tommath.h:
	make -C third-party/libtommath install DESTDIR=$$(pwd)/$(BUILDDIR)/third-party/libtommath CFLAGS="$(CFLAGS_TP) $(CFLAGS_LTM)" LIBPATH=/lib INCPATH=/include CROSS_COMPILE=$(TOOLCHAIN_PREFIX)

$(BUILDDIR)/third-party/libtomcrypt/include/tomcrypt.h: $(BUILDDIR)/third-party/libtommath/include/tommath.h
$(BUILDDIR)/third-party/libtomcrypt/lib/libtomcrypt.a: $(BUILDDIR)/third-party/libtommath/include/tommath.h
	make -C third-party/libtomcrypt install DESTDIR=$$(pwd)/$(BUILDDIR)/third-party/libtomcrypt CFLAGS="$(CFLAGS_TP) $(CFLAGS_LTC) -DARGTYPE=1" LIBPATH=/lib INCPATH=/include CROSS_COMPILE=$(TOOLCHAIN_PREFIX)
