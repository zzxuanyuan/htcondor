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
# Setup additional command-line arguments to configure
############################################################

AC_ARG_WITH(externals,
 AC_HELP_STRING([--with-externals=DIR],
   [Root of directory tree for external programs needed for building Condor (no default, must be specified)]),
 ac_cv_externals=$withval,
 AC_MSG_ERROR([you must use --with-externals=DIR to specify the path to the directory tree for external programs needed to build Condor]))
AC_SUBST(ext_root,$ac_cv_externals)

AC_ARG_WITH(purecachedir,
 AC_HELP_STRING([--with-purecachedir=DIR],
   [cache directory for objects instrumented with Purify (default is /tmp)]),
 ac_cv_purecachedir=$withval,
 ac_cv_purecachedir=/tmp)
AC_SUBST(pure_cache_dir,$ac_cv_purecachedir)


############################################################
# checks for programs
############################################################

AC_LANG(C)
AC_PROG_CC

AC_LANG(C++)
AC_PROG_CXX

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
