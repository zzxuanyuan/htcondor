#set the local target 
set(TargetName utils)

#set the local 
file(GLOB HeaderFiles "*.h" )
file(GLOB SourceFiles "*.cpp")
file(GLOB WinSrcs "*INDOW*")

#file(GLOB TestSrcs "test*")
#list(REMOVE_ITEM SourceFiles ${TestSrcs})

# need to if wrap windows
if(MSVC)
    #?
else()
    list(REMOVE_ITEM SourceFiles ${WinSrcs})
    list(REMOVE_ITEM HeaderFiles ${WinSrcs})
endif()

#create the library
add_library(${TargetName} STATIC ${HeaderFiles} ${SourceFiles})

