@echo off
REM 
REM Top level script that executes the building of the externals for Win32.
REM

REM First, set some environment variables.
call set_vars.bat

REM Now have Perl do the work of building all the damn things.

REM build gsoap-2.6
echo Building gsoap-2.6
perl -w %EXTERN_DIR%\build_external --extern_dir=%EXTERN_DIR% --package_name=gsoap-2.6 --extern_config=%cd%

REM build kerberos
REM ...

REM now just exit with the return code of the last command
exit /b %ERRORLEVEL%
