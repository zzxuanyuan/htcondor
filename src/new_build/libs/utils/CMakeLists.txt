#set the local target 
set(TargetName utils)

include_directories(${CONDOR_SOURCE_DIR}/libs/utils)

# items to be removed from the compile.
# platform specific files will be handled by condor_glob
file( GLOB RmvSrcs soap_helpers* )

condor_glob( HeaderFiles SourceFiles "${RmvSrcs}" )

#create the library
add_library(${TargetName} STATIC ${HeaderFiles} ${SourceFiles})
