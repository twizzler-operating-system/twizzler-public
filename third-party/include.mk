CFLAGS_LTM=-DMP_NO_DEV_URANDOM
CFLAGS_LTC=-DLTM_DESC -DLTC_NO_FILE -DLTC_NO_ASM

CFLAGS+=$(CFLAGS_LTM) $(CFLAGS_LTC)

INCLUDES+=-I$(BUILDDIR)/third-party/libtommath/include
INCLUDES+=-I$(BUILDDIR)/third-party/libtomcrypt/include

KLIBS=$(BUILDDIR)/third-party/libtommath/lib/libtommath.a
KLIBS=$(BUILDDIR)/third-party/libtomcrypt/lib/libtomcrypt.a

$(BUILDDIR)/third-party/libtommath/include/tommath.h:
$(BUILDDIR)/third-party/libtommath/lib/libtommath.a:
	make -C third-party/libtommath install DESTDIR=$$(pwd)/$(BUILDDIR)/third-party/libtommath CFLAGS="$(CFLAGS_LTM)" LIBPATH=/lib INCPATH=/include

$(BUILDDIR)/third-party/libtomcrypt/include/tomcrypt.h:
$(BUILDDIR)/third-party/libtomcrypt/lib/libtomcrypt.a:
	make -C third-party/libtomcrypt install DESTDIR=$$(pwd)/$(BUILDDIR)/third-party/libtomcrypt CFLAGS="$(CFLAGS_LTC)" LIBPATH=/lib INCPATH=/include
