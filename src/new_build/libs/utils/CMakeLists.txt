#set the local target 
set(TargetName utils)

#set the local 
file(GLOB HeaderFiles *.h)
file(GLOB SourceFiles *.c)

#create the library
add_library(${TargetName} STATIC ${HeaderFiles} ${SourceFiles})

