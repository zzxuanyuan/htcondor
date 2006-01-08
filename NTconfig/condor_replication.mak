# Microsoft Developer Studio Generated NMAKE File, Based on condor_replication.dsp
!IF "$(CFG)" == ""
CFG=condor_replication - Win32 Debug
!MESSAGE No configuration specified. Defaulting to condor_replication - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "condor_replication - Win32 Release" && "$(CFG)" != "condor_replication - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_replication.mak" CFG="condor_replication - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_replication - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "condor_replication - Win32 Debug" (based on "Win32 (x86) Console Application")
!MESSAGE 
!ERROR An invalid configuration is specified.
!ENDIF 

!IF "$(OS)" == "Windows_NT"
NULL=
!ELSE 
NULL=nul
!ENDIF 

CPP=cl.exe
RSC=rc.exe

!IF  "$(CFG)" == "condor_replication - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

ALL : "$(OUTDIR)\condor_replication.exe"


CLEAN :
	-@erase "$(INTDIR)\AbstractReplicatorStateMachine.obj"
	-@erase "$(INTDIR)\FilesOperations.obj"
	-@erase "$(INTDIR)\Replication.obj"
	-@erase "$(INTDIR)\ReplicatorStateMachine.obj"
	-@erase "$(INTDIR)\Utils.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\Version.obj"
	-@erase "$(OUTDIR)\condor_replication.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /ML /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /Fp"$(INTDIR)\condor_replication.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_replication.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /incremental:no /pdb:"$(OUTDIR)\condor_replication.pdb" /machine:I386 /out:"$(OUTDIR)\condor_replication.exe" 
LINK32_OBJS= \
	"$(INTDIR)\AbstractReplicatorStateMachine.obj" \
	"$(INTDIR)\FilesOperations.obj" \
	"$(INTDIR)\Replication.obj" \
	"$(INTDIR)\ReplicatorStateMachine.obj" \
	"$(INTDIR)\Utils.obj" \
	"$(INTDIR)\Version.obj"

"$(OUTDIR)\condor_replication.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "condor_replication - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "$(OUTDIR)\condor_replication.exe"


CLEAN :
	-@erase "$(INTDIR)\AbstractReplicatorStateMachine.obj"
	-@erase "$(INTDIR)\FilesOperations.obj"
	-@erase "$(INTDIR)\Replication.obj"
	-@erase "$(INTDIR)\ReplicatorStateMachine.obj"
	-@erase "$(INTDIR)\Utils.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(INTDIR)\Version.obj"
	-@erase "$(OUTDIR)\condor_replication.exe"
	-@erase "$(OUTDIR)\condor_replication.ilk"
	-@erase "$(OUTDIR)\condor_replication.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MLd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /Fp"$(INTDIR)\condor_replication.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_replication.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /incremental:yes /pdb:"$(OUTDIR)\condor_replication.pdb" /debug /machine:I386 /out:"$(OUTDIR)\condor_replication.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\AbstractReplicatorStateMachine.obj" \
	"$(INTDIR)\FilesOperations.obj" \
	"$(INTDIR)\Replication.obj" \
	"$(INTDIR)\ReplicatorStateMachine.obj" \
	"$(INTDIR)\Utils.obj" \
	"$(INTDIR)\Version.obj"

"$(OUTDIR)\condor_replication.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ENDIF 

.c{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.obj::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.c{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cpp{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<

.cxx{$(INTDIR)}.sbr::
   $(CPP) @<<
   $(CPP_PROJ) $< 
<<


!IF "$(NO_EXTERNAL_DEPS)" != "1"
!IF EXISTS("condor_replication.dep")
!INCLUDE "condor_replication.dep"
!ELSE 
!MESSAGE Warning: cannot find "condor_replication.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "condor_replication - Win32 Release" || "$(CFG)" == "condor_replication - Win32 Debug"
SOURCE=..\src\condor_had\AbstractReplicatorStateMachine.C

"$(INTDIR)\AbstractReplicatorStateMachine.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_had\FilesOperations.C

"$(INTDIR)\FilesOperations.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_had\Replication.C

"$(INTDIR)\Replication.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_had\ReplicatorStateMachine.C

"$(INTDIR)\ReplicatorStateMachine.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_had\Utils.C

"$(INTDIR)\Utils.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_had\Version.C

"$(INTDIR)\Version.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

