@echo off
REM Build Condor from a batch file
REM Todd Tannenbaum <tannenba@cs.wisc.edu> Feb 2002

REM Make all environment changes local
setlocal

REM We want to be able to make the build exit with an exit code
REM instead of setting ERRORLEVEL, if, say, we're calling the bat file
REM from Perl, which doesn't understand ERRORLEVEL.
set INTERACTIVE=/b
IF "%1" == "/exit" set INTERACTIVE=

REM Although we have it as a rule in the .dsp files, somehow our prebuild 
REM rule for syscall_numbers.h gets lost into the translation to .mak files, 
REM so we deal with it here explicitly.
if not exist ..\src\h\syscall_numbers.h awk -f ..\src\h\awk_prog.include_file ..\src\h\syscall_numbers.tmpl > ..\src\h\syscall_numbers.h

REM Build the externals
call make_win32_externals.bat
if %ERRORLEVEL% NEQ 0 goto failure

REM Copy any .dll files created by the externals in debug and release
call copy_external_dlls.bat
if %ERRORLEVEL% NEQ 0 goto failure

if defined INCLUDE goto :check_sdk
call VCVARS32.BAT
if defined INCLUDE goto :check_sdk
echo *** Visual C++ bin directory not in the path, or compiler not installed.
goto failure
:check_sdk
if defined MSSDK goto :compiler_ready
call setenv /2000 /RETAIL
if defined MSSDK goto :compiler_ready
echo *** Microsoft SDK directory not in the path, or not installed.
goto failure
:compiler_ready
set conf=Release
if /i A%1==Arelease shift
if /i A%1==Adebug (set conf=Debug& shift & echo Debug Build - Output going to ..\Debug) else (echo Release Build - Output going to ..\Release)
call dorenames.bat > NUL
if not errorlevel 2 call dorenames.bat > NUL

rem this gsoap should probably be wrapped up into a VC project too, but 
rem for now this will do
echo *** Building gsoap & echo . & nmake /U /C /f gsoap.mak RECURSE="0" CFG="gsoap - Win32 %conf%" %* || goto failure
rem Build condor (build order is now preserved in project)
msbuild condor.sln /t:condor /p:Configuration=%conf% || goto failure 

echo .
echo *** Done.  Build is all happy.  Congrats!  Go drink beer.  
exit %INTERACTIVE% 0
:failure
echo .
echo *** Build Stopped.  Please fix errors and try again.
exit %INTERACTIVE% 1
