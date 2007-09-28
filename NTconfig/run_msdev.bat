@echo off
REM Prepare to build Condor.

REM Keep all environment changes local to this script
setlocal

call dorenames.bat > NUL
if not errorlevel 2 call dorenames.bat > NUL

REM Although we have it as a rule in the .dsp files, somehow our prebuild 
REM rule for syscall_numbers.h gets lost into the translation to .mak files, 
REM so we deal with it here explicitly.
if not exist ..\src\h\syscall_numbers.h awk -f ..\src\h\awk_prog.include_file ..\src\h\syscall_numbers.tmpl > ..\src\h\syscall_numbers.h

REM Build the externals
call make_win32_externals.bat
if %ERRORLEVEL% NEQ 0 goto extfail

REM Copy any .dll files created by the externals in debug and release
call copy_external_dlls.bat
if %ERRORLEVEL% NEQ 0 goto extfail

REM Deal with gsoap
nmake /f gsoap.mak

REM make_win32_externals implicitly calls set_vars.bat, so just run
REM dev studio as long as the extenals build ok.
if not gsoap%ERRORLEVEL% == gsoap0 goto failure

rem If we are using vc8 we may be using the free express version, so check for
rem if it exists after we try the real vc8 launcher.
if exist "%DevEnvDir%\devenv.exe" (
    cmd /k "%DevEnvDir%\devenv.exe" /useenv condor.sln 
) else ( 
    echo Is Visual Studio installed?!
    goto failure
)

rem else if exist "%DevEnvDir%\VCExpress.exe" (
rem         "%DevEnvDir%\VCExpress.exe" /useenv condor.sln
rem    

goto success

:failure
echo *** gsoap stub generator failed ***
exit /b 1
:extfail
echo *** Failed to build externals ***
exit /b 1

:success


