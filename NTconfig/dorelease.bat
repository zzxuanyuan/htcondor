@echo off
setlocal
if exist %1 goto process1
echo Error - directory %1 does not exist
echo Usage: dorelease.bat path_to_release_subdirectory
goto end
:process1
if exist ..\Release goto process2
echo Compile a Release Build of Condor first.
goto end
:process2

rem Set up environment
call set_vars.bat

REM Set up release-dir directories
echo Creating Release Directory...
if not exist %1\bin\NUL mkdir %1\bin
if not exist %1\lib\NUL mkdir %1\lib
if not exist %1\lib\webservice\NUL mkdir %1\lib\webservice
if not exist %1\etc\NUL mkdir %1\etc
echo Copying files...
copy ..\Release\*.exe %1\bin
copy ..\Release\*.dll %1\bin
copy ..\Release\*.pdb %1\bin
copy ..\src\condor_starter.V6.1\*.class %1\lib
copy ..\src\condor_starter.V6.1\*.jar %1\lib
copy ..\src\condor_chirp\Chirp.jar %1\lib
copy ..\src\condor_examples\condor_config.* %1\etc
copy pdh.dll %1\bin
pushd .
cd ..\src
for /R %%f in (*.wsdl) do copy %%f %1\lib\webservice
popd

echo Cleaning out debug symbols that are not required ...
for %%f in (master startd quill had credd schedd collector negotiator shadow starter) do echo %%f && move condor_%%f.pdb condor_%%f.save
del *.pdb
for %%f in (master startd quill had credd schedd collector negotiator shadow starter) do move condor_%%f.save condor_%%f.pdb

copy condor_rm.exe condor_hold.exe
copy condor_rm.exe condor_release.exe
copy condor_rm.exe condor_vacate_job.exe
copy condor.exe condor_off.exe
copy condor.exe condor_on.exe
copy condor.exe condor_restart.exe
copy condor.exe condor_reconfig.exe
copy condor.exe condor_reschedule.exe
copy condor.exe condor_vacate.exe
copy condor_cod.exe condor_cod_request.exe

cd ..
if not exist include\NUL mkdir include
copy %EXT_INSTALL%\%EXT_DRMAA_VERSION%\include\* include
copy %EXT_INSTALL%\%EXT_DRMAA_VERSION%\lib\* lib
if not exist src\NUL mkdir src
if not exist src\drmaa\NUL mkdir src\drmaa
copy %EXT_INSTALL%\%EXT_DRMAA_VERSION%\src\* src\drmaa
popd

:end
