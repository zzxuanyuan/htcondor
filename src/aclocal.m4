dnl CHECK_GNU_MAKE available from the GNU Autoconf Macro Archive at:
dnl http://www.gnu.org/software/ac-archive/htmldoc/check_gnu_make.html
dnl Modified by Derek Wright 10/6/03 to improve reporting.  Also,
dnl since Condor depends on GNU make, we do not need to be using
dnl AC_SUBST() at all, since we're just going to bail out of configure
dnl if this fails.
AC_DEFUN( [CHECK_GNU_MAKE],
 [AC_CACHE_CHECK( [for GNU make], [_cv_gnu_make_command],
   _cv_gnu_make_command='not found' ;
   dnl Search all the common names for GNU make
   for a in "$MAKE" make gmake gnumake ; do
      if test -z "$a" ; then continue ; fi ;
      if ( sh -c "$a --version" 2> /dev/null | grep GNU  2>&1 > /dev/null ) ;
      then
         _cv_gnu_make_command=$a ;
         break;
      fi
   done ;
 );

 dnl If there was a GNU version, then set @ifGNUmake@ to the empty
 dnl string, '#' otherwise 
 if test  "$_cv_gnu_make_command" != "not found"  ; then
     ifGNUmake='' ;
 else
     ifGNUmake='#' ;
     _cv_gnu_make_command='';
 fi
] )
