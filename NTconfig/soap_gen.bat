@echo off

REM Batch file to generate soap stubs.
REM
REM usage: soap_gen <soap_module>

set BATCHMODE=/b

REM Only set this if its not already defined.
if A%SOAPCPP2%==A set SOAPCPP2=soapcpp2.exe
set SOAPCPP2FLAGS= -I ..\condor_daemon_core.V6
set TEMPDIR=%TEMP%\_soap_gen_tmp
set SOAPMODULE=%1
set SOAPHEADERFILE=gsoap_%SOAPMODULE%.h
set STUBFILELIST=soap_%SOAPMODULE%C.cpp soap_%SOAPMODULE%Server.cpp condor%SOAPMODULE%.nsmap soap_%SOAPMODULE%H.h soap_%SOAPMODULE%Stub.h condor%SOAPMODULE%.wsdl

REM First check if we have everything we need in the path or cwd.

if not A%SOAPMODULE%==A goto module_ok
echo Please specify a soap module to build stubs for (e.g. COLLECTOR)
exit %BATCHMODE% 1

:module_ok
if exist %SOAPHEADERFILE% goto headerfile_ok
echo .
echo ERROR: %SOAPHEADERFILE% not in current directory. Aborting!
exit %BATCHMODE% 1

:headerfile_ok
%SOAPCPP2% -h > NUL 2>&1
if %errorlevel% EQU 0 goto stdsoapcpp_ok
echo .
echo ERROR: %SOAPCPP2% not in PATH. Aborting!
exit %BATCHMODE% 1

:stdsoapcpp_ok
REM everything's cool, so create the tempdir
rd /q /s %TEMPDIR% > NUL 2>&1
mkdir %TEMPDIR%
if %errorlevel% EQU 0 goto tempdir_ok
echo .
echo ERROR: Failed to create temp dir %TEMPDIR%. Aborting!
exit %BATCHMODE% 1

:tempdir_ok
%SOAPCPP2% %SOAPCPP2FLAGS% -p soap_%SOAPMODULE% -d %TEMPDIR% %SOAPHEADERFILE%

for %%f in ( %STUBFILELIST% ) do ( copy %TEMPDIR%\%%f . || (set FAILEDFILE=%%f & goto copyfailure) )

copy /Y *.cpp *.C
del *.cpp
rmdir /S /Q %TEMPDIR%
echo .
echo Success! Soap stubs generated.
exit %BATCHMODE%  0

:copyfailure
rmdir /S /Q %TEMPDIR%
echo .
echo ERROR: Failed to copy
echo     %TEMPDIR%\%FAILEDFILE%
echo to the current directory. Aborting.
exit %BATCHMODE% 1

