#set the local target 
set(TargetName sysapi)

file( GLOB SysApiRmvElements *_t* *.t.* dhry21b* )

condor_glob(SysapiHeaderFiles SysapiSourceFiles "${SysApiRmvElements}" )

#create the library
add_library(${TargetName} STATIC ${SysapiHeaderFiles} ${SysapiSourceFiles})
