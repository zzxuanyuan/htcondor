@echo off
REM ======================================================================
REM First set all the variables that contain common bits for our build
REM environment.
REM * * * * * * * * * * * * * PLEASE READ* * * * * * * * * * * * * * * *
REM Microsoft Visual Studio will choke on variables that contain strings 
REM exceeding 255 chars, so be careful when editing this file! It's 
REM totally lame but there's nothing we can do about it.
REM ======================================================================

REM Where do the completed externals live?
if A%EXTERN_DIR%==A  set EXTERN_DIR=%cd%\..\externals
set EXT_INSTALL=%EXTERN_DIR%\install

REM Specify which versions of the externals we're using
set EXT_GSOAP_VERSION=gsoap-2.6
set EXT_KERBEROS_VERSION=
set EXT_GLOBUS_VERSION=


REM Put NTConfig in the PATH, since it's got lots of stuff we need
REM like awk, gunzip, tar, bison, yacc...
set PATH=%cd%;%SystemRoot%;%SystemRoot%\system32;C:\Perl\bin;"C:\Program Files\Microsoft Visual Studio\VC98\bin";"C:\Program Files\Microsoft SDK"
call vcvars32.bat
call setenv.bat /2000 /RETAIL

REM Set up some stuff for BISON
set BISON_SIMPLE=%cd%\bison.simple
set BISON_HAIRY=%cd%\bison.hairy

REM Tell the build system where we can find soapcpp2
set SOAPCPP2=%EXT_INSTALL%\%EXT_GSOAP_VERSION%\soapcpp2.exe

set CONDOR_INCLUDE=/I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /I "..\src\condor_daemon_core.V6" 
set CONDOR_LIB=Crypt32.lib mpr.lib psapi.lib mswsock.lib netapi32.lib imagehlp.lib advapi32.lib ws2_32.lib user32.lib
REM The following is just a place holder. This can not be left blank.
set CONDOR_LIBPATH=

REM ======================================================================
REM Now set the individual variables specific to each external package.
REM Some have been defined, but are not in use yet.
REM ======================================================================

REM ** GSOAP
set CONDOR_GSOAP_INCLUDE=
set CONDOR_GSOAP_LIB=
set CONDOR_GSOAP_LIBPATH=

REM ** GLOBUS
set CONDOR_GLOBUS_INCLUDE=
set CONDOR_GLOBUS_LIB=
set CONDOR_GLOBUS_LIBPATH=

REM ** KERBEROS
set CONDOR_KERB_INCLUDE=
set CONDOR_KERB_LIB=
set CONDOR_KERB_LIBPATH=

REM ** PCRE
set CONDOR_PCRE_INCLUDE=
set CONDOR_PCRE_LIB=
set CONDOR_PCRE_LIBPATH=

