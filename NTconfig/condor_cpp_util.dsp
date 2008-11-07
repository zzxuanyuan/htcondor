# Microsoft Developer Studio Project File - Name="condor_cpp_util" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=condor_cpp_util - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "condor_cpp_util.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_cpp_util.mak" CFG="condor_cpp_util - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_cpp_util - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "condor_cpp_util - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "Debug"
# PROP BASE Intermediate_Dir "Debug"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\Debug"
# PROP Intermediate_Dir "..\Debug"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /W3 /GX /Z7 /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /YX /FD /c
# ADD CPP /nologo /MTd /W3 /Gm /Gi /GX /ZI /Od /I "..\src\condor_tt" /D "WIN32" /D "_DEBUG" /Fp"..\Debug\condor_common.pch" /Yu"condor_common.h" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_DEFINES) $(CONDOR_CPPARGS) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "condor_cpp_util___Win32_Release"
# PROP BASE Intermediate_Dir "condor_cpp_util___Win32_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\Release"
# PROP Intermediate_Dir "..\Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /GX /Z7 /Od /D "WIN32" /D "_DEBUG" /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h" /FD /TP /c
# SUBTRACT BASE CPP /Fr
# ADD CPP /nologo /MT /W3 /GX /Z7 /O1 /I "..\src\condor_tt" /D "WIN32" /D "NDEBUG" /Fp"..\Release\condor_common.pch" /Yu"condor_common.h" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_DEFINES) $(CONDOR_CPPARGS) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ENDIF 

# Begin Target

# Name "condor_cpp_util - Win32 Debug"
# Name "condor_cpp_util - Win32 Release"
# Begin Source File

SOURCE="..\src\condor_c++_util\access.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\access.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\ad_printmask.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\ad_printmask.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\build_job_env.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\build_job_env.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\check_events.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\check_events.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\classad_collection.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\classad_collection.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\classad_command_util.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\classad_command_util.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\classad_hashtable.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\classad_hashtable.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\classad_helpers.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\classad_helpers.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\classad_log.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\classad_log.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\classad_merge.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\classad_merge.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\classad_namedlist.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\classad_visa.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\classad_visa.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\command_strings.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\command_strings.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\ConcurrencyLimitUtils.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\ConcurrencyLimitUtils.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_arglist.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_arglist.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_attributes.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_common.cc"

!IF  "$(CFG)" == "condor_cpp_util - Win32 Debug"

# PROP Intermediate_Dir "..\Debug"
# ADD CPP /Gi- /Yc"condor_common.h"

!ELSEIF  "$(CFG)" == "condor_cpp_util - Win32 Release"

# PROP Intermediate_Dir "..\Release"
# ADD CPP /Gi- /Yc"condor_common.h"

!ENDIF 

# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_config.cc"
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_config.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_cron.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_cronjob.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_cronmgr.h
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_crontab.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_crontab.h"
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_client\condor_daemon_client.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_distribution.h
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_environ.cc"
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_environ.h
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_event.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_event.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_ftp.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_getcwd.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_getcwd.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_id.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_id.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_md.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_open.cc"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_open.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_parameters.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_q.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_q.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_query.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_query.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_state.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_url.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_url.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_user_policy.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_user_policy.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_ver_info.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_ver_info.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\condor_version.cc"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\CondorError.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\CondorError.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\config.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\cron.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\cronjob.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\cronjob_classad.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\cronmgr.cc"
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_client\daemon.cc
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_client\daemon.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_client\daemon_list.cc
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_client\daemon_list.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_client\daemon_types.cc
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_client\daemon_types.h
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\date_util.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\date_util.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\dbms_utils.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\dbms_utils.h"
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_client\dc_collector.cc
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_client\dc_collector.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_client\dc_message.cc
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_client\dc_message.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_client\dc_schedd.cc
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_client\dc_schedd.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_client\dc_shadow.cc
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_client\dc_shadow.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_client\dc_startd.cc
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_client\dc_startd.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_client\dc_starter.cc
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_client\dc_starter.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_client\dc_transfer_queue.cc
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_client\dc_transfer_queue.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_client\dc_transferd.cc
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_client\dc_transferd.h
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\directory.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\directory.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\directory.WINDOWS.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\directory.WINDOWS.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\distribution.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\domain_tools.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\domain_tools.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\dynuser.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\dynuser.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\email_cpp.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\enum_utils.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\enum_utils.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\env.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\env.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\error_utils.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\extArray.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\extra_param_info.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\extra_param_info.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\file_lock.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\file_sql.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\file_sql.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\file_transfer.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\file_transfer.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\file_transfer_db.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\file_transfer_db.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\file_xml.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\file_xml.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\filename_tools_cpp.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\forkwork.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\forkwork.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\format_time.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\format_time.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\gahp_common.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\gahp_common.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\generic_query.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\generic_query.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\get_daemon_name.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\get_daemon_name.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\get_full_hostname.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\HashTable.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\HashTable.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\hibernation_manager.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\hibernation_manager.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\hibernator.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\hibernator.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\hibernator.WINDOWS.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\hibernator.WINDOWS.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\historysnapshot.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\historysnapshot.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\hook_utils.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\hook_utils.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\iso_dates.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\iso_dates.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\java_config.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\java_config.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\job_lease.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\job_lease.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\jobqueuesnapshot.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\jobqueuesnapshot.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\KeyCache.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\killfamily.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\killfamily.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\linebuffer.cc"
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\linebuffer.h
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\list.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\load_dll.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\load_dll.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\log.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\log.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\log_transaction.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\log_transaction.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\lsa_mgr.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\lsa_mgr.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\MapFile.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\MapFile.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\metric_units.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\metric_units.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\misc_utils.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\my_distribution.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\my_dynuser.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\my_hostname.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\my_popen.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\my_popen.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\my_subsystem.cc"
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\my_username.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\my_username.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\MyString.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\MyString.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\net_string_list.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\net_string_list.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\network_adapter.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\network_adapter.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\network_adapter.WINDOWS.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\network_adapter.WINDOWS.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\ntsysinfo.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\ntsysinfo.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\perm.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\perm.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\pgsqldatabase.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\pgsqldatabase.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\print_wrapped_text.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\print_wrapped_text.h"
# End Source File
# Begin Source File

SOURCE=..\src\h\proc.h
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\proc_family_direct.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\proc_family_direct.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\proc_family_interface.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\proc_family_interface.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\proc_family_proxy.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\proc_family_proxy.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\proc_id.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\procd_config.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\procd_config.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\process_control.WINDOWS.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\process_control.WINDOWS.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\profile.WINDOWS.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\profile.WINDOWS.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\query_result_type.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\Queue.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\read_multiple_logs.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\read_multiple_logs.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\read_user_log.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\read_user_log_state.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\read_user_log_state.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\Regex.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\remote_close.WINDOWS.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\remote_close.WINDOWS.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\security.WINDOWS.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\security.WINDOWS.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\selector.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\selector.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\Set.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\set_user_priv_from_ad.cc"
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\set_user_priv_from_ad.h
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\setenv.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\setenv.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\setup_api_dll.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\setup_api_dll.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\sig_name.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\sig_name.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\simplelist.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\sqlquery.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\sqlquery.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\stat_info.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\stat_info.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\stat_wrapper.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\stat_wrapper.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\stat_wrapper_internal.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\stat_wrapper_internal.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\status_string.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\status_string.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\store_cred.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\store_cred.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\string_conversion.WINDOWS.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\string_conversion.WINDOWS.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\string_list.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\string_list.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\stringSpace.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\stringSpace.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\strnewp.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\subsystem_info.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\subsystem_info.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\system_info.WINDOWS.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\system_info.WINDOWS.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\time_offset.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\time_offset.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\tmp_dir.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\tmp_dir.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\token_cache.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\token_cache.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\transfer_request.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\translation_utils.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\translation_utils.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\udp_waker.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\udp_waker.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\uids.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\usagemon.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\user_job_policy.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\user_job_policy.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\user_log.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\user_log.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\user_log_header.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\user_log.c++.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\utc_time.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\utc_time.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\vm_univ_utils.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\vm_univ_utils.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\waker.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\waker.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\which.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\which.h"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\windows_firewall.cc"
# End Source File
# Begin Source File

SOURCE="..\src\condor_c++_util\windows_firewall.h"
# End Source File
# End Target
# End Project
