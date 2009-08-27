#set the local target 
set(TargetName utils)

#set the local 
file(GLOB HeaderFiles *.h)
file(GLOB SourceFiles *.cpp)

# Temporary for cleanup
add_definitions(-DLINUX=LINUX)

set(WANT_LEASE_MANAGER TRUE)
set(GLIBC29 "GLIBC29")
set(C_COMP_KIND_GCC TRUE)

#-DLINUX=LINUX         -DWANT_LEASE_MANAGER   -pthread -g -Wall -W -Wextra -Wfloat-equal -Wshadow -Wendif-labels -Wpointer-arith -Wcast-qual #-Wcast-align -Wvolatile-register-var -fstack-protector  -DGLIBC=GLIBC -DGLIBC29=GLIBC29 -DC_COMP_KIND_GCC -DC_COMP_MAJOR_GCC_4 #-DC_COMP_MINOR_GCC_4_3 -DC_COMP_PATCH_GCC_4_3_2 -DWITH_OPENSSL -DCOMPILE_SOAP_SSL   -pthread -Wnested-externs -Wstrict-prototypes #-Wmissing-prototypes

#create the library
add_library(${TargetName} STATIC ${HeaderFiles} ${SourceFiles})

