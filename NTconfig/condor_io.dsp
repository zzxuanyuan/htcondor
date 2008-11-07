# Microsoft Developer Studio Project File - Name="condor_io" - Package Owner=<4>
# Microsoft Developer Studio Generated Build File, Format Version 6.00
# ** DO NOT EDIT **

# TARGTYPE "Win32 (x86) Static Library" 0x0104

CFG=condor_io - Win32 Release
!MESSAGE This is not a valid makefile. To build this project using NMAKE,
!MESSAGE use the Export Makefile command and run
!MESSAGE 
!MESSAGE NMAKE /f "condor_io.mak".
!MESSAGE 
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_io.mak" CFG="condor_io - Win32 Release"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_io - Win32 Debug" (based on "Win32 (x86) Static Library")
!MESSAGE "condor_io - Win32 Release" (based on "Win32 (x86) Static Library")
!MESSAGE 

# Begin Project
# PROP AllowPerConfigDependencies 0
# PROP Scc_ProjName ""
# PROP Scc_LocalPath ""
CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "condor_io - Win32 Debug"

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
# ADD CPP /nologo /MTd /W3 /Gm /Gi /GX /Zi /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /Fp"..\Debug\condor_common.pch" /Yu"condor_common.h" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c
# ADD BASE RSC /l 0x409
# ADD RSC /l 0x409
BSC32=bscmake.exe
# ADD BASE BSC32 /nologo
# ADD BSC32 /nologo
LIB32=link.exe -lib
# ADD BASE LIB32 /nologo
# ADD LIB32 /nologo

!ELSEIF  "$(CFG)" == "condor_io - Win32 Release"

# PROP BASE Use_MFC 0
# PROP BASE Use_Debug_Libraries 1
# PROP BASE Output_Dir "condor_io___Win32_Release"
# PROP BASE Intermediate_Dir "condor_io___Win32_Release"
# PROP BASE Target_Dir ""
# PROP Use_MFC 0
# PROP Use_Debug_Libraries 1
# PROP Output_Dir "..\Release"
# PROP Intermediate_Dir "..\Release"
# PROP Target_Dir ""
# ADD BASE CPP /nologo /MDd /W3 /GX /Z7 /Od /D "WIN32" /D "_DEBUG" /D "_WINDOWS" /Fp"..\src\condor_c++_util/condor_common.pch" /Yu"condor_common.h" /FD /TP /c
# SUBTRACT BASE CPP /Fr
# ADD CPP /nologo /MT /W3 /GX /Z7 /O1 /D "WIN32" /D "NDEBUG" /D "_WINDOWS" /Fp"..\Release\condor_common.pch" /Yu"condor_common.h" /FD /TP $(CONDOR_INCLUDE) $(CONDOR_GSOAP_INCLUDE) $(CONDOR_GLOBUS_INCLUDE) $(CONDOR_KERB_INCLUDE) $(CONDOR_PCRE_INCLUDE) $(CONDOR_OPENSSL_INCLUDE) $(CONDOR_POSTGRESQL_INCLUDE) /c
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

# Name "condor_io - Win32 Debug"
# Name "condor_io - Win32 Release"
# Begin Source File

SOURCE=..\src\condor_io\authentication.cc
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\authentication.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\buffers.cc
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\buffers.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\cedar_no_ckpt.cc
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\condor_auth.cc
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_auth.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\condor_auth_anonymous.cc
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\condor_auth_claim.cc
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_auth_claim.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\condor_auth_kerberos.cc
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\condor_auth_passwd.cc
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_auth_passwd.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\condor_auth_ssl.cc
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_auth_ssl.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\condor_auth_sspi.cc
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_auth_sspi.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\condor_crypt.cc
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_crypt.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\condor_crypt_3des.cc
# ADD CPP /Yu
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\condor_crypt_blowfish.cc
# SUBTRACT CPP /YX /Yc /Yu
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\condor_io.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_core.V6\condor_ipverify.cc
# End Source File
# Begin Source File

SOURCE=..\src\condor_daemon_core.V6\condor_ipverify.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\condor_rw.cc
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\condor_rw.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\condor_secman.cc
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\CryptKey.cc
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\CryptKey.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\errno_num.cc
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\open_flags.c
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\reli_sock.cc
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\reli_sock.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\safe_sock.cc
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\safe_sock.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\SafeMsg.cc
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\SafeMsg.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\sock.cc
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\sock.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\sockCache.cc
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\sockCache.h
# End Source File
# Begin Source File

SOURCE=..\src\condor_io\stream.cc
# End Source File
# Begin Source File

SOURCE=..\src\condor_includes\stream.h
# End Source File
# End Target
# End Project
