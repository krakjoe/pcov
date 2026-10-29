dnl $Id$
dnl config.m4 for extension pcov

PHP_ARG_ENABLE(pcov, whether to enable php coverage support,
[  --enable-pcov           Enable php coverage support])

if test "$PHP_PCOV" != "no"; then
  PHP_VERSION=$($PHP_CONFIG --vernum)

  AC_MSG_CHECKING(PHP version)

  if test $PHP_VERSION -lt 80600; then
    AC_MSG_ERROR([pcov requires PHP 8.6+])
  fi

  AC_MSG_RESULT(8.6+)
  PHP_NEW_EXTENSION(pcov, pcov.c, $ext_shared,, -DZEND_ENABLE_STATIC_TSRMLS_CACHE=1 -std=gnu99)
fi
