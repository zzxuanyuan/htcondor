@echo off
if exist ..\src\condor_master.V6\daemon.cc goto happywindows
echo Making happy for the repository...
move ..\src\condor_master.V6\daemon_master.cc ..\src\condor_master.V6\daemon.cc
move ..\src\condor_starter.V6.1\starter_class.cc ..\src\condor_starter.V6.1\starter.cc
move ..\src\condor_util_lib\condor_common_c.c ..\src\condor_util_lib\condor_common.c
move ..\src\condor_c++_util\email_cpp.cc ..\src\condor_c++_util\email.cc
move ..\src\condor_mail\condor_email_main.cc ..\src\condor_mail\main.cc
move ..\src\condor_eventd\eventd_main.cc ..\src\condor_eventd\main.cc
move ..\src\condor_dagman\dagman_submit.cc ..\src\condor_dagman\submit.cc
move ..\src\condor_dagman\dagman_util.cc ..\src\condor_dagman\util.cc
move ..\src\condor_had\had_Version.cc ..\src\condor_had\Version.cc
exit /B 1
:happywindows
echo Making happy for the Windows build...
move ..\src\condor_master.V6\daemon.cc ..\src\condor_master.V6\daemon_master.cc
move ..\src\condor_starter.V6.1\starter.cc ..\src\condor_starter.V6.1\starter_class.cc
move ..\src\condor_util_lib\condor_common.c ..\src\condor_util_lib\condor_common_c.c
move ..\src\condor_c++_util\email.cc ..\src\condor_c++_util\email_cpp.cc
move ..\src\condor_mail\main.cc ..\src\condor_mail\condor_email_main.cc
move ..\src\condor_eventd\main.cc ..\src\condor_eventd\eventd_main.cc
move ..\src\condor_dagman\submit.cc ..\src\condor_dagman\dagman_submit.cc
move ..\src\condor_dagman\util.cc ..\src\condor_dagman\dagman_util.cc
move ..\src\condor_had\Version.cc ..\src\condor_had\had_Version.cc
exit /B 2
