dnl $Id$
dnl config.m4 for extension cartoon

dnl Comments in this file start with the string 'dnl'.
dnl Remove where necessary. This file will not work
dnl without editing.

dnl If your extension references something external, use with:

dnl PHP_ARG_WITH(cartoon, for cartoon support,
dnl Make sure that the comment is aligned:
dnl [  --with-cartoon             Include cartoon support])

dnl Otherwise use enable:

dnl PHP_ARG_ENABLE(cartoon, whether to enable cartoon support,
dnl Make sure that the comment is aligned:
dnl [  --enable-cartoon           Enable cartoon support])

if test "$PHP_CARTOON" != "no"; then
  dnl Write more examples of tests here...

  dnl # --with-cartoon -> check with-path
  dnl SEARCH_PATH="/usr/local /usr"     # you might want to change this
  dnl SEARCH_FOR="/include/cartoon.h"  # you most likely want to change this
  dnl if test -r $PHP_CARTOON/$SEARCH_FOR; then # path given as parameter
  dnl   CARTOON_DIR=$PHP_CARTOON
  dnl else # search default path list
  dnl   AC_MSG_CHECKING([for cartoon files in default path])
  dnl   for i in $SEARCH_PATH ; do
  dnl     if test -r $i/$SEARCH_FOR; then
  dnl       CARTOON_DIR=$i
  dnl       AC_MSG_RESULT(found in $i)
  dnl     fi
  dnl   done
  dnl fi
  dnl
  dnl if test -z "$CARTOON_DIR"; then
  dnl   AC_MSG_RESULT([not found])
  dnl   AC_MSG_ERROR([Please reinstall the cartoon distribution])
  dnl fi

  dnl # --with-cartoon -> add include path
  dnl PHP_ADD_INCLUDE($CARTOON_DIR/include)

  dnl # --with-cartoon -> check for lib and symbol presence
  dnl LIBNAME=cartoon # you may want to change this
  dnl LIBSYMBOL=cartoon # you most likely want to change this 

  dnl PHP_CHECK_LIBRARY($LIBNAME,$LIBSYMBOL,
  dnl [
  dnl   PHP_ADD_LIBRARY_WITH_PATH($LIBNAME, $CARTOON_DIR/$PHP_LIBDIR, CARTOON_SHARED_LIBADD)
  dnl   AC_DEFINE(HAVE_CARTOONLIB,1,[ ])
  dnl ],[
  dnl   AC_MSG_ERROR([wrong cartoon lib version or lib not found])
  dnl ],[
  dnl   -L$CARTOON_DIR/$PHP_LIBDIR -lm
  dnl ])
  dnl
  dnl PHP_SUBST(CARTOON_SHARED_LIBADD)

  PHP_NEW_EXTENSION(cartoon, cartoon.c, $ext_shared)
fi
