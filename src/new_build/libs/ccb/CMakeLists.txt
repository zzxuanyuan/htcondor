#set the local target 
set(TargetName ccb)

#file( GLOB ccbRmvSrcs  )

condor_glob(ccbHdrs ccbSrcs "")

#create the library
add_library(${TargetName} STATIC ${ccbHdrs} ${ccbSrcs})

target_link_libraries(${TargetName} utils)
