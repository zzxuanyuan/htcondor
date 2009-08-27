#################################
# Set project and reqs
project(CONDOR)
cmake_minimum_required(VERSION 2.6)

##################################
# Setup package meta-data
set(PACKAGE "condor")
set(VERSION "7.X.X")
set(PACKAGE_TARNAME "")
set(PACKAGE_BUGREPORT " ")
set(PACKAGE_NAME "condor")
set(PACKAGE_VERSION "-")
set(PACKAGE_STRING "condor-")
#SET(PACKAGE_BUGREPORT "a suitable mailing list =>http://...")
set(OPERATING_SYSTEM "${CMAKE_SYSTEM_NAME}")

##################################
# Include options used for custom macros which we wish to create
# list(APPEND CMAKE_MODULE_PATH ${CONDOR_SOURCE_DIR}/tools/cmake)

#################################
# The following are options passed into CMAKE allowing, for 
# custom configuration, similar to configure.  

#option(BGD "Enable support for HTC mode on the BlueGene machine." OFF )
#option(MD5 "Enable production and validation of MD5 checksums for released
#packages." OFF )
#option(SHA1 "Enable production and validation of SHA1 checksums." OFF)
#option(PROPER "Force this configure script to work in the proper (non-Condor)
#way." OFF )
# option(FULLPORT "." OFF ) this build CMAKE and on will only be clipped
#option( JOBHOOKS "Support for invoking hooks throughout the workflow of a job." OFF )
#option( HIBERNATION "Support for Condor-controlled hibernation." OFF )
#option( "." OFF )
#option( "." OFF )
#option( "." OFF )
#option( "." OFF )
#option( "." OFF )
## TYPICAL OUTPUT 
#-DHAVE_CONFIG_H -DX86_64=X86_64 -DLINUX=LINUX         -DWANT_LEASE_MANAGER
#-pthread -g -Wall -W -Wextra -Wfloat-equal -Wshadow -Wendif-labels
#-Wpointer-arith -Wcast-qual -Wcast-align -Wvolatile-register-var
#-fstack-protector  -DGLIBC=GLIBC -DGLIBC29=GLIBC29 -DC_COMP_KIND_GCC
#-DC_COMP_MAJOR_GCC_4 -DC_COMP_MINOR_GCC_4_3 -DC_COMP_PATCH_GCC_4_3_2
#-DWITH_OPENSSL -DCOMPILE_SOAP_SSL   -pthread

##################################
# Build dynamic libraries unless explicitly stated
set(BUILD_SHARED_LIBS FLASE)
# Supress automatic regeneration of project files
set(CMAKE_SUPPRESS_REGENERATION FALSE)

# Set build location for targets
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

# Add executable's directory to its library search path
set(CMAKE_INSTALL_RPATH ".")
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)

# Find third-party packages (or externals)
#find_package(Boost REQUIRED thread)
#find_package(PythonLibs REQUIRED)

# Set compiler flags
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /WX /wd4251 /wd4275 /wd4996")
else()
    #add_definitions(-D_LINUX)
endif()

include_directories(${CONDOR_SOURCE_DIR}/include)
include_directories(${CONDOR_SOURCE_DIR}/include)/utils

# Add components in dependency order
add_subdirectory(libs/utils)

