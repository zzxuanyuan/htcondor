# CHECK_MAKE_IS_GNU is based loosely on the CHECK_GNU_MAKE macro 
# available from the GNU Autoconf Macro Archive at:
# http://www.gnu.org/software/ac-archive/htmldoc/check_gnu_make.html
# Modified by Derek Wright 10/6/03 to improve reporting, and to define 
# more appropriate variables given that Condor depends on the "make"
# in your PATH being GNU make...
AC_DEFUN( [CHECK_MAKE_IS_GNU],
[AC_CHECK_PROG(MAKE,make,make)
 if test "$MAKE" != "make"; then
   AC_MSG_ERROR( [make is required] )
 fi
 AC_CACHE_CHECK( [if $MAKE is GNU make], [_cv_make_is_gnu],
 [if ( sh -c "$MAKE --version" 2> /dev/null | grep GNU  2>&1 > /dev/null ) ;
  then
     _cv_make_is_gnu="yes"
  else
     _cv_make_is_gnu="no"
  fi
 ])
])


dnl Available from the GNU Autoconf Macro Archive at:
dnl http://www.gnu.org/software/ac-archive/htmldoc/ac_prog_perl_version.html
dnl
AC_DEFUN([AC_PROG_PERL_VERSION],[dnl
# Make sure we have perl
if test -z "$PERL"; then
  AC_CHECK_PROG(PERL,perl,perl)
fi
# Check if version of Perl is sufficient
ac_perl_version="$1"
if test "x$PERL" != "x"; then
  AC_MSG_CHECKING(for perl version greater than or equal to $ac_perl_version)
  # NB: It would be nice to log the error if there is one, but we cannot rely
  # on autoconf internals
  $PERL -e "use $ac_perl_version;" > /dev/null 2>&1
  if test $? -ne 0; then
    AC_MSG_RESULT(no);
    $3
  else
    AC_MSG_RESULT(ok);
    $2
  fi
else
  AC_MSG_WARN(could not find perl)
fi
])dnl


