dnl $Id$
dnl config.m4 for extension pcov

PHP_ARG_ENABLE(pcov, whether to enable php coverage support,
[  --enable-pcov           Enable php coverage support])

if test "$PHP_PCOV" != "no"; then
  PHP_VERSION=$($PHP_CONFIG --vernum)

  AC_MSG_CHECKING(PHP version)

  if test $PHP_VERSION -gt 80099; then
    PHP_NEW_EXTENSION(pcov, pcov.c, $ext_shared,, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)
  else
      if test $PHP_VERSION -lt 70100; then
        AC_MSG_ERROR([pcov supports PHP 7.1+])
      elif test $PHP_VERSION -lt 70200; then
        AC_MSG_RESULT(7.1)
        PHP_PCOV_CFG_VERSION=701
      elif test $PHP_VERSION -lt 70300; then
        AC_MSG_RESULT(7.2)
        PHP_PCOV_CFG_VERSION=702
      elif test $PHP_VERSION -lt 70400; then
        AC_MSG_RESULT(7.3)
        PHP_PCOV_CFG_VERSION=703
      else
        AC_MSG_RESULT(7.4+)
        PHP_PCOV_CFG_VERSION=704
      fi
      
    PHP_NEW_EXTENSION(pcov, pcov.c cfg/$PHP_PCOV_CFG_VERSION/zend_cfg.c, $ext_shared,, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1)
    PHP_ADD_BUILD_DIR($ext_builddir/cfg/$PHP_PCOV_CFG_VERSION, 1)
    PHP_ADD_INCLUDE($ext_srcdir/cfg/$PHP_PCOV_CFG_VERSION)
  fi
fi
