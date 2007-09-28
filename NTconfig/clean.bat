@echo off

rem Set up environment
call set_vars.bat

rem determine which build to clean
set conf=Release
if /i A%1==Arelease shift
if /i A%1==Adebug (set conf=Debug)

rem clean the project files
msbuild condor.sln /t:condor:clean /p:Configuration=%conf%

rem could also clean the externals too...
rem EXT_INSTALL
rem EXT_TRIGGERS