@echo off
if exist ..\src\condor_master.V6\daemon.cpp goto happywindows
echo Making happy for the repository...
move ..\src\condor_master.V6\daemon_master.cpp ..\src\condor_master.V6\daemon.cpp
move ..\src\condor_starter.V6.1\starter_class.cpp ..\src\condor_starter.V6.1\starter.cpp
REM move ..\src\condor_util_lib\condor_common_c.c ..\src\condor_util_lib\condor_common.c
move ..\src\condor_c++_util\email_cpp.cpp ..\src\condor_c++_util\email.cpp
REM move ..\src\condor_mail\condor_email_main.cpp ..\src\condor_mail\main.cpp
move ..\src\condor_eventd\eventd_main.cpp ..\src\condor_eventd\main.cpp
move ..\src\condor_dagman\dagman_submit.cpp ..\src\condor_dagman\submit.cpp
move ..\src\condor_dagman\dagman_util.cpp ..\src\condor_dagman\util.cpp
move ..\src\condor_had\had_Version.cpp ..\src\condor_had\Version.cpp
exit /B 1
:happywindows
echo Making happy for the Windows build...
move ..\src\condor_master.V6\daemon.cpp ..\src\condor_master.V6\daemon_master.cpp
move ..\src\condor_starter.V6.1\starter.cpp ..\src\condor_starter.V6.1\starter_class.cpp
REM move ..\src\condor_util_lib\condor_common.c ..\src\condor_util_lib\condor_common_c.c
move ..\src\condor_c++_util\email.cpp ..\src\condor_c++_util\email_cpp.cpp
REM move ..\src\condor_mail\main.cpp ..\src\condor_mail\condor_email_main.cpp
move ..\src\condor_eventd\main.cpp ..\src\condor_eventd\eventd_main.cpp
move ..\src\condor_dagman\submit.cpp ..\src\condor_dagman\dagman_submit.cpp
move ..\src\condor_dagman\util.cpp ..\src\condor_dagman\dagman_util.cpp
move ..\src\condor_had\Version.cpp ..\src\condor_had\had_Version.cpp
exit /B 2
