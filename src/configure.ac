# Configure template for Condor.                             -*-Autoconf-*- 
# Process this file with autoconf to produce a configure script.
###########################################################################
# CONDOR Copyright Notice
#
# See LICENSE.TXT for additional notices and disclaimers.
#
# Copyright (c)1990-2003 CONDOR Team, Computer Sciences Department, 
# University of Wisconsin-Madison, Madison, WI.  All Rights Reserved.  
# No use of the CONDOR Software Program Source Code is authorized 
# without the express consent of the CONDOR Team.  For more information 
# contact: CONDOR Team, Attention: Professor Miron Livny, 
# 7367 Computer Sciences, 1210 W. Dayton St., Madison, WI 53706-1685, 
# (608) 262-0856 or miron@cs.wisc.edu.
#
# U.S. Government Rights Restrictions: Use, duplication, or disclosure 
# by the U.S. Government is subject to restrictions as set forth in 
# subparagraph (c)(1)(ii) of The Rights in Technical Data and Computer 
# Software clause at DFARS 252.227-7013 or subparagraphs (c)(1) and 
# (2) of Commercial Computer Software-Restricted Rights at 48 CFR 
# 52.227-19, as applicable, CONDOR Team, Attention: Professor Miron 
# Livny, 7367 Computer Sciences, 1210 W. Dayton St., Madison, 
# WI 53706-1685, (608) 262-0856 or miron@cs.wisc.edu.
###########################################################################

# We need a recent autoconf for many of the macros and tests we're using
AC_PREREQ(2.57)

# Initialize autoconf
AC_INIT

############################################################
# Check for command-line arguments and environment variables
############################################################

AC_ARG_WITH(externals,
 AC_HELP_STRING([--with-externals=DIR],
   [Root of directory tree for external programs needed for building Condor (default is to search in ../externals)]),
 [if test $withval = no; then
    AC_MSG_WARN([You can not use --without-externals or use "no"])
    AC_MSG_ERROR([--with-externals must define a directory (see --help)])
  fi
  CONDOR_VERIFY_EXTERNALS_DIR([$withval],
    [directory specified with --with-externals ($withval) is not valid])
 ],
 [
  # nothing specified, try ../externals.  however, instead of using
  # "`pwd`/../externals", we use AS_DIRNAME to strip off the "src"
  # part of `pwd` and then tack on "/externals".  This doesn't get
  # things right if there are weird symlinks going on, but it wouldn't
  # work if we were using "../externals", either.  In the general case
  # where there's a real parent directory holding src, config and
  # externals, everything works great, and using this as a default
  # makes it possible to just run "./configure" most of the time.
  _cur_pwd=`pwd`
  _root_dir=`AS_DIRNAME(["$_cur_pwd"])`
  CONDOR_VERIFY_EXTERNALS_DIR([$_root_dir/externals],
  [../externals is not a valid directory tree for external programs needed to build Condor, you must use --with-externals=DIR to specify the path])
 ]
)
AC_SUBST(ext_root,$ac_cv_externals)

AC_ARG_VAR(TMPDIR, [Directory for temp files during Condor build [/tmp]])
if test "$ac_cv_env_TMPDIR_set" != set; then
    TMPDIR=/tmp
fi
if test ! -d $TMPDIR || test ! -w $TMPDIR; then
    AC_MSG_ERROR([TMPDIR ($TMPDIR) is not a writable directory])
fi

AC_ARG_WITH(purecachedir,
 AC_HELP_STRING([--with-purecachedir=DIR],
  [cache directory for objects instrumented with Purify (default is $TMPDIR)]),
 [ac_cv_purecachedir=$withval],
 [ac_cv_purecachedir="$TMPDIR/$USER/.pcache"]
)
AC_SUBST(pure_cache_dir,$ac_cv_purecachedir)


############################################################
# checks for programs
############################################################

CHECK_PROG_IS_GNU(MAKE, make)
if test $_cv_make_is_gnu = no; then
   AC_MSG_ERROR( [GNU make is required] )
fi

AC_LANG(C)
AC_PROG_CC
if test $ac_cv_c_compiler_gnu != yes; then
   AC_MSG_ERROR( [GNU CC (gcc) is required] )
fi   

AC_LANG(C++)
AC_PROG_CXX
if test $ac_cv_cxx_compiler_gnu != yes; then
   AC_MSG_ERROR( [GNU C++ (g++) is required] )
fi   


# Condor only requires 5.002, but GPT apparently relies on 5.005_03 or
# greater.  Unfortunately, GPT doesn't have a configure script or
# anything, it just lets perl die with a require statement.  So, we
# test for the right version here and fail in an early and obvious way
# if there's a problem.
AC_PROG_PERL_VERSION([5.005_03], [],
   [AC_MSG_ERROR([perl version 5.005_03 or higher is required])])

AC_PROG_LEX
AC_CHECK_PROG(ac_lex,lex,yes,no)
if test $ac_lex = no; then
    AC_MSG_ERROR( [lex is required] )
fi

AC_CHECK_PROG(ac_yacc,yacc,yes,no)
if test $ac_yacc = no; then
    AC_MSG_ERROR( [yacc is required] )
fi

# ar is used for numerous libraries in Condor, and for extracting
# objects from the system libc.a for the Condor syscall library.
AC_CHECK_PROG(ac_ar,ar,yes,no)
if test $ac_ar = no; then
    AC_MSG_ERROR( [ar is required] )
fi

# strip is used in the packaging of Condor for all released binaries
# we require GNU strip since it does a much better job of shrinking
# the size of our binaries, given that they're all built with GNU CC.
CHECK_PROG_IS_GNU(STRIP, strip)
if test "$_cv_strip_is_gnu" = "no"; then
   AC_MSG_ERROR( [GNU strip is required] )
fi

# m4 is used to build PVM
AC_CHECK_PROG(ac_m4prog,m4,yes,no)
if test $ac_m4prog = no; then
    AC_MSG_ERROR( [m4 is required] )
fi

AC_CHECK_PROG(ac_purify,purify,yes,no)
if test "$ac_purify" == yes; then
   AC_MSG_CHECKING([purify cache directory ($ac_cv_purecachedir)])
   if test -d $ac_cv_purecachedir && test -w $ac_cv_purecachedir ; then
      AC_MSG_RESULT( exists )
   else
      AS_MKDIR_P($ac_cv_purecachedir)
      AC_MSG_RESULT( created )
   fi
fi


# checks for libraries

# checks for header files

# checks for types

# checks for structures

# checks for compiler characteristics

# checks for library functions

# checks for system services


############################################################
# Configure-generated output
############################################################
AC_OUTPUT(../config/externals.cf)

# Finally, call Condor's imake script to generate Makefile using imake
# (yes, we know it seems crazy to use configure to setup Imake to make
# Makefiles... we're in transition).
./condor_imake
