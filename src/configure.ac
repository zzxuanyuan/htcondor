AC_INIT
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
AC_SUBST(pure_cache_dir)
AC_OUTPUT(../config/externals.cf)
./condor_imake
