# Microsoft Developer Studio Generated NMAKE File, Based on condor_transferer.dsp
!IF "$(CFG)" == ""
CFG=condor_transferer - Win32 Debug
!MESSAGE No configuration specified. Defaulting to condor_transferer - Win32 Debug.
!ENDIF 

!IF "$(CFG)" != "condor_transferer - Win32 Release" && "$(CFG)" != "condor_transferer - Win32 Debug"
!MESSAGE Invalid configuration "$(CFG)" specified.
!MESSAGE You can specify a configuration when running NMAKE
!MESSAGE by defining the macro CFG on the command line. For example:
!MESSAGE 
!MESSAGE NMAKE /f "condor_transferer.mak" CFG="condor_transferer - Win32 Debug"
!MESSAGE 
!MESSAGE Possible choices for configuration are:
!MESSAGE 
!MESSAGE "condor_transferer - Win32 Release" (based on "Win32 (x86) Console Application")
!MESSAGE "condor_transferer - Win32 Debug" (based on "Win32 (x86) Console Application")
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

!IF  "$(CFG)" == "condor_transferer - Win32 Release"

OUTDIR=.\Release
INTDIR=.\Release
# Begin Custom Macros
OutDir=.\Release
# End Custom Macros

ALL : "$(OUTDIR)\condor_transferer.exe"


CLEAN :
	-@erase "$(INTDIR)\BaseReplicaTransferer.obj"
	-@erase "$(INTDIR)\DownloadReplicaTransferer.obj"
	-@erase "$(INTDIR)\FilesOperations.obj"
	-@erase "$(INTDIR)\Transferer.obj"
	-@erase "$(INTDIR)\UploadReplicaTransferer.obj"
	-@erase "$(INTDIR)\Utils.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(OUTDIR)\condor_transferer.exe"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /ML /W3 /GX /O2 /D "WIN32" /D "NDEBUG" /D "_CONSOLE" /D "_MBCS" /Fp"$(INTDIR)\condor_transferer.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_transferer.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib  kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /incremental:no /pdb:"$(OUTDIR)\condor_transferer.pdb" /machine:I386 /out:"$(OUTDIR)\condor_transferer.exe" 
LINK32_OBJS= \
	"$(INTDIR)\Transferer.obj" \
	"$(INTDIR)\UploadReplicaTransferer.obj" \
	"$(INTDIR)\Utils.obj" \
	"$(INTDIR)\BaseReplicaTransferer.obj" \
	"$(INTDIR)\DownloadReplicaTransferer.obj" \
	"$(INTDIR)\FilesOperations.obj"

"$(OUTDIR)\condor_transferer.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
    $(LINK32) @<<
  $(LINK32_FLAGS) $(LINK32_OBJS)
<<

!ELSEIF  "$(CFG)" == "condor_transferer - Win32 Debug"

OUTDIR=.\Debug
INTDIR=.\Debug
# Begin Custom Macros
OutDir=.\Debug
# End Custom Macros

ALL : "$(OUTDIR)\condor_transferer.exe"


CLEAN :
	-@erase "$(INTDIR)\BaseReplicaTransferer.obj"
	-@erase "$(INTDIR)\DownloadReplicaTransferer.obj"
	-@erase "$(INTDIR)\FilesOperations.obj"
	-@erase "$(INTDIR)\Transferer.obj"
	-@erase "$(INTDIR)\UploadReplicaTransferer.obj"
	-@erase "$(INTDIR)\Utils.obj"
	-@erase "$(INTDIR)\vc60.idb"
	-@erase "$(INTDIR)\vc60.pdb"
	-@erase "$(OUTDIR)\condor_transferer.exe"
	-@erase "$(OUTDIR)\condor_transferer.ilk"
	-@erase "$(OUTDIR)\condor_transferer.pdb"

"$(OUTDIR)" :
    if not exist "$(OUTDIR)/$(NULL)" mkdir "$(OUTDIR)"

CPP_PROJ=/nologo /MLd /W3 /Gm /GX /ZI /Od /D "WIN32" /D "_DEBUG" /D "_CONSOLE" /D "_MBCS" /Fp"$(INTDIR)\condor_transferer.pch" /YX /Fo"$(INTDIR)\\" /Fd"$(INTDIR)\\" /FD /GZ  /c 
BSC32=bscmake.exe
BSC32_FLAGS=/nologo /o"$(OUTDIR)\condor_transferer.bsc" 
BSC32_SBRS= \
	
LINK32=link.exe
LINK32_FLAGS=kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib  kernel32.lib user32.lib gdi32.lib winspool.lib comdlg32.lib advapi32.lib shell32.lib ole32.lib oleaut32.lib uuid.lib odbc32.lib odbccp32.lib /nologo /subsystem:console /incremental:yes /pdb:"$(OUTDIR)\condor_transferer.pdb" /debug /machine:I386 /out:"$(OUTDIR)\condor_transferer.exe" /pdbtype:sept 
LINK32_OBJS= \
	"$(INTDIR)\Transferer.obj" \
	"$(INTDIR)\UploadReplicaTransferer.obj" \
	"$(INTDIR)\Utils.obj" \
	"$(INTDIR)\BaseReplicaTransferer.obj" \
	"$(INTDIR)\DownloadReplicaTransferer.obj" \
	"$(INTDIR)\FilesOperations.obj"

"$(OUTDIR)\condor_transferer.exe" : "$(OUTDIR)" $(DEF_FILE) $(LINK32_OBJS)
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
!IF EXISTS("condor_transferer.dep")
!INCLUDE "condor_transferer.dep"
!ELSE 
!MESSAGE Warning: cannot find "condor_transferer.dep"
!ENDIF 
!ENDIF 


!IF "$(CFG)" == "condor_transferer - Win32 Release" || "$(CFG)" == "condor_transferer - Win32 Debug"
SOURCE=..\src\condor_had\BaseReplicaTransferer.C

"$(INTDIR)\BaseReplicaTransferer.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_had\DownloadReplicaTransferer.C

"$(INTDIR)\DownloadReplicaTransferer.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_had\FilesOperations.C

"$(INTDIR)\FilesOperations.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_had\Transferer.C

"$(INTDIR)\Transferer.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_had\UploadReplicaTransferer.C

"$(INTDIR)\UploadReplicaTransferer.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)


SOURCE=..\src\condor_had\Utils.C

"$(INTDIR)\Utils.obj" : $(SOURCE) "$(INTDIR)"
	$(CPP) $(CPP_PROJ) $(SOURCE)



!ENDIF 

