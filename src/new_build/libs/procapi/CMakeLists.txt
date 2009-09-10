#set the local target 
set(TargetName procapi)

file( GLOB ProcAPISrcs procapi.cpp processid.cpp procapi_killfamily.cpp )

# there are a series of other targets as well
# including a test binary and test exe's...
# procinterface being one of them.

#create the library
add_library ( ${TargetName} STATIC ${ProcAPISrcs} )
#target_link_libraries(${TargetName} utils )
