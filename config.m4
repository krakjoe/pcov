dnl $Id$
dnl config.m4 for extension pcov

PHP_ARG_ENABLE(pcov, whether to enable php coverage support,
[  --enable-pcov           Enable php coverage support])

if test "$PHP_PCOV" != "no"; then

  PHP_NEW_EXTENSION(pcov, pcov.c cfg/zend_cfg.c, $ext_shared,, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)

  PHP_ADD_BUILD_DIR($ext_builddir/cfg, 1)
  PHP_ADD_INCLUDE($ext_builddir/cfg)
fi
