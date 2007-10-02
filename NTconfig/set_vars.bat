@echo off
REM ======================================================================
REM First set all the variables that contain common bits for our build
REM environment.
REM * * * * * * * * * * * * * PLEASE READ* * * * * * * * * * * * * * * *
REM Microsoft Visual Studio will choke on variables that contain strings 
REM exceeding 255 chars, so be careful when editing this file! It's 
REM totally lame but there's nothing we can do about it.
REM ======================================================================

rem For externals: it just tells them we are building with VC2005
rem (In the future, when we do not have VC6, we can remove this--
rem after we, of coutse, remove any reference to them from the externals)
set USING_VC8_TO_BUILD=True

REM Set paths to Visual C++, the Platform SDKs, and Perl
REM NOTE: we assume that everything has been installed in the default location
REM       if this is not the case, then you will need to change these paths
set VC_DIR=%SystemDrive%\Program Files\Microsoft Visual Studio 8\VC\Bin
echo Using VC_DIR: %VC_DIR% & echo.
set SDK_DIR=%SystemDrive%\Program Files\Microsoft Platform SDK
set SDK_XP_DIR=%SystemDrive%\Program Files\Microsoft Platform SDK for Windows XP SP2
set PERL_DIR=%SystemDrive%\Perl\bin

REM Where do the completed externals live?
if A%EXTERN_DIR%==A  set EXTERN_DIR=%cd%\..\externals
set EXT_INSTALL=%EXTERN_DIR%\install
set EXT_TRIGGERS=%EXTERN_DIR%\triggers

REM Specify which versions of the externals we're using. To add a 
REM new external, just add its version here, and add that to the 
REM EXTERNALS_NEEDED variable defined below.
set EXT_GSOAP_VERSION=gsoap-2.7.6c
set EXT_OPENSSL_VERSION=openssl-0.9.8
set EXT_POSTGRESQL_VERSION=postgresql-8.0.2
set EXT_KERBEROS_VERSION=krb5-1.4.3
set EXT_GLOBUS_VERSION=
set EXT_PCRE_VERSION=pcre-5.0
set EXT_DRMAA_VERSION=drmaa-1.4

REM Now tell the build system what externals we need built.
set EXTERNALS_NEEDED=%EXT_GSOAP_VERSION% %EXT_OPENSSL_VERSION% %EXT_KERBEROS_VERSION% %EXT_PCRE_VERSION% %EXT_POSTGRESQL_VERSION% %EXT_DRMAA_VERSION%

REM Put NTConfig in the PATH, since it's got lots of stuff we need
REM like awk, gunzip, tar, bison, yacc... 
set PATH=%cd%;%SystemRoot%;%SystemRoot%\system32;%PERL_DIR%;%VC_DIR%;%SDK_DIR%;%SDK_XP_DIR%

call vcvars32.bat
if not defined INCLUDE ( echo . && echo *** Failed to run VCVARS32.BAT! Is Microsoft Visual Studio installed? && exit /B 1 )
call setenv /2000 /RETAIL
if not defined MSSDK ( echo . && echo *** Failed to run SETENV.BAT! Is Microsoft Platform SDK installed? && exit /B 1 )

REM Set up some stuff for BISON
set BISON_SIMPLE=%cd%\bison.simple
set BISON_HAIRY=%cd%\bison.hairy

REM Tell the build system where we can find soapcpp2
set SOAPCPP2=%EXT_INSTALL%\%EXT_GSOAP_VERSION%\soapcpp2.exe

set CONDOR_INCLUDE=/I "..\src\h" /I "..\src\condor_includes" /I "..\src\condor_c++_util" /I "..\src\condor_daemon_client" /I "..\src\condor_daemon_core.V6" /I "..\src\condor_schedd.V6" /GR
set CONDOR_LIB=Crypt32.lib mpr.lib psapi.lib mswsock.lib netapi32.lib imagehlp.lib advapi32.lib ws2_32.lib user32.lib oleaut32.lib ole32.lib
set CONDOR_LIBPATH=

REM Tell VC makefiles that we do not wish to use external dependency
REM (.dep) files.
set NO_EXTERNAL_DEPS=1

REM ======================================================================
REM Now set the individual variables specific to each external package.
REM Some have been defined, but are not in use yet.
rem
rem [BCB] I've commented out the ones we don't use--namely globus--if in 
rem the future we do use them, you will also need to add them back into 
rem the project files as they were removed from there as well since they 
rem served only to give warnings for years upon years.
rem
rem _INCLUDE ones go in C/C++ > Command Line > Additional options:
rem _LIB* ones go in Linker > Input > Additional Dependencies
rem
REM ======================================================================

REM ** GSOAP
set CONDOR_GSOAP_INCLUDE=/I %EXT_INSTALL%\%EXT_GSOAP_VERSION%\src /DHAVE_BACKFILL=1 /DHAVE_BOINC=1 /DCONDOR_G=1 /DWITH_OPENSSL=1 /DCOMPILE_SOAP_SSL=1
set CONDOR_GSOAP_LIB=
set CONDOR_GSOAP_LIBPATH=

REM ** GLOBUS
rem set CONDOR_GLOBUS_INCLUDE=
rem set CONDOR_GLOBUS_LIB=
rem set CONDOR_GLOBUS_LIBPATH=

REM ** OPENSSL
set CONDOR_OPENSSL_INCLUDE=/I %EXT_INSTALL%\%EXT_OPENSSL_VERSION%\inc32 /D CONDOR_BLOWFISH_ENCRYPTION /D CONDOR_MD /D CONDOR_ENCRYPTION /D CONDOR_3DES_ENCRYPTION /D SSL_AUTHENTICATION
set CONDOR_OPENSSL_LIB=libeay32.lib ssleay32.lib
set CONDOR_OPENSSL_LIBPATH=/LIBPATH:%EXT_INSTALL%\%EXT_OPENSSL_VERSION%\out32dll
rem set CONDOR_OPENSSL_LIBPATH=/LIBPATH:%EXT_INSTALL%\%EXT_OPENSSL_VERSION%\out32dll /NODEFAULTLIB:LIBCMT.LIB

REM ** POSTGRESQL
set CONDOR_POSTGRESQL_INCLUDE=/I %EXT_INSTALL%\%EXT_POSTGRESQL_VERSION%\inc32 /D WANT_QUILL
set CONDOR_POSTGRESQL_LIB=libpqdll.lib
set CONDOR_POSTGRESQL_LIBPATH=/LIBPATH:%EXT_INSTALL%\%EXT_POSTGRESQL_VERSION%\out32dll

REM ** KERBEROS
set CONDOR_KERB_INCLUDE=/I %EXT_INSTALL%\%EXT_KERBEROS_VERSION%\include /D KERBEROS_AUTHENTICATION 
set CONDOR_KERB_LIB=comerr32.lib gssapi32.lib k5sprt32.lib krb5_32.lib xpprof32.lib
set CONDOR_KERB_LIBPATH=/LIBPATH:%EXT_INSTALL%\%EXT_KERBEROS_VERSION%\lib

REM ** PCRE
set CONDOR_PCRE_INCLUDE=/I %EXT_INSTALL%\%EXT_PCRE_VERSION%\include
set CONDOR_PCRE_LIB=libpcre.lib
set CONDOR_PCRE_LIBPATH=/LIBPATH:%EXT_INSTALL%\%EXT_PCRE_VERSION%\lib

exit /B 0
