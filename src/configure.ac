AC_INIT

AC_ARG_WITH([purecachedir],AC_HELP_STRING([--with-purecachedir=DIR],[cache directory for objects instrumented with Purify (default is /tmp)]),ac_cv_purecachedir=$withval,[ac_cv_purecachedir=/tmp])
AC_SUBST(pure_cache_dir,$ac_cv_purecachedir)

AC_LANG(C)
AC_PROG_LEX
AC_CHECK_PROG(ac_lex,lex,yes,no)
if test "$ac_lex" == no; then
	echo "lex is required";
	exit;
fi
AC_CHECK_PROG(ac_yacc,yacc,yes,no)
if test "$ac_yacc" == no; then
	echo "yacc is required";
	exit;
fi
AC_PROG_CC
AC_LANG(C++)
AC_PROG_CXX
AC_OUTPUT(../config/externals.cf)
./condor_imake
