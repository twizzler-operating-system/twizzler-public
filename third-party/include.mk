CFLAGS_LTM=-DMP_NO_DEV_URANDOM -DMP_NO_FILE
CFLAGS_LTC=-DLTM_DESC -DLTC_NO_FILE -DLTC_NO_ASM -DMP_NO_FILE -DARGTYPE=1
CFLAGS_LTC_DEFS=-DXMALLOC=kalloc -DXFREE=kfree -DXCALLOC=kcalloc

CFLAGS+=$(CFLAGS_LTM) $(CFLAGS_LTC)
CFLAGS_TP=$(CORE_CFLAGS) $(addprefix -I../../,$(INCLUDE_DIRS))

INCLUDE_DIRS+=$(BUILDDIR)/third-party/libtommath/include
INCLUDE_DIRS+=$(BUILDDIR)/third-party/libtomcrypt/include

KLIBS+=$(BUILDDIR)/third-party/libtomcrypt/lib/libtomcrypt.a
KLIBS+=$(BUILDDIR)/third-party/libtommath/lib/libtommath.a

$(BUILDDIR)/third-party/libtommath/lib/libtommath.a:
$(BUILDDIR)/third-party/libtommath/include/tommath.h:
	make -C third-party/libtommath install DESTDIR=$$(pwd)/$(BUILDDIR)/third-party/libtommath CFLAGS="$(CFLAGS_TP) $(CFLAGS_LTM) $(CFLAGS_LTM_DEFS)" LIBPATH=/lib INCPATH=/include CROSS_COMPILE=$(TOOLCHAIN_PREFIX)

$(BUILDDIR)/third-party/libtomcrypt/lib/libtomcrypt.a $(BUILDDIR)/third-party/libtomcrypt/include/tomcrypt_custom.h $(BUILDDIR)/third-party/libtomcrypt/include/tomcrypt_cfg.h $(BUILDDIR)/third-party/libtomcrypt/include/tomcrypt_macros.h $(BUILDDIR)/third-party/libtomcrypt/include/tomcrypt_cipher.h $(BUILDDIR)/third-party/libtomcrypt/include/tomcrypt_hash.h $(BUILDDIR)/third-party/libtomcrypt/include/tomcrypt_mac.h $(BUILDDIR)/third-party/libtomcrypt/include/tomcrypt_prng.h $(BUILDDIR)/third-party/libtomcrypt/include/tomcrypt_pk.h $(BUILDDIR)/third-party/libtomcrypt/include/tomcrypt_math.h $(BUILDDIR)/third-party/libtomcrypt/include/tomcrypt_misc.h $(BUILDDIR)/third-party/libtomcrypt/include/tomcrypt_argchk.h $(BUILDDIR)/third-party/libtomcrypt/include/tomcrypt_pkcs.h $(BUILDDIR)/third-party/libtomcrypt/include/tomcrypt.h: $(BUILDDIR)/third-party/libtommath/include/tommath.h
	make -C third-party/libtomcrypt install DESTDIR=$$(pwd)/$(BUILDDIR)/third-party/libtomcrypt CFLAGS="$(CFLAGS_TP) $(CFLAGS_LTC) $(CFLAGS_LTC_DEFS)" LIBPATH=/lib INCPATH=/include CROSS_COMPILE=$(TOOLCHAIN_PREFIX)
