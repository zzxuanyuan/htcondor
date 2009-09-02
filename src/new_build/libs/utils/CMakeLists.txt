#set the local target 
set(TargetName utils)

condor_glob(HeaderFiles SourceFiles)

#create the library
add_library(${TargetName} STATIC ${HeaderFiles} ${SourceFiles})
