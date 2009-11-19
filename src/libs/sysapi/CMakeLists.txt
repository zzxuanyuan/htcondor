
file( GLOB SysApiRmvElements *_t.cpp *.t.* dhry21b* )

condor_glob(SysapiHeaderFiles SysapiSourceFiles "${SysApiRmvElements}" )

#create the library
add_library(sysapi STATIC ${SysapiHeaderFiles} ${SysapiSourceFiles})