###########################################################################
# Set project and reqs
project(CONDOR)
cmake_minimum_required(VERSION 2.6)

include (CheckIncludeFiles)
include (CheckFunctionExists)
include (CheckSymbolExists)
include (CheckLibraryExists)

###########################################################################
# Setup package meta-data
set(PACKAGE "condor")
set(VERSION "7.X.X")
set(PACKAGE_TARNAME "")
set(PACKAGE_BUGREPORT " ")
set(PACKAGE_NAME "condor")
set(PACKAGE_VERSION "-")
set(PACKAGE_STRING "condor-")
SET(PACKAGE_BUGREPORT "a suitable mailing list =>http://...")
string( TOUPPER "${CMAKE_SYSTEM_NAME}" OPERATING_SYSTEM )

###########################################################################
# The following are the configure options
# checking X... Y.... Z...
message(STATUS "##########################################################################")
message(STATUS "Condor's cmake is now configuring based on your env & input options")
##################################################
# Beginning with environment elements which are tacked on
message(STATUS "Operating System: ${OPERATING_SYSTEM}")
add_definitions(-D${OPERATING_SYSTEM}=${OPERATING_SYSTEM})
add_definitions(-DCONDOR_${OPERATING_SYSTEM}=${OPERATING_SYSTEM})
#checking glibc version stuff and tacking on.
add_definitions(-DGLIBC=GLIBC)

#I'm ignoring this check add_definitions(-DGLIBC29=GLIBC29)
#add_definitions(-DC_COMP_KIND_GCC)
#add_definitions(-DC_COMP_MAJOR_GCC_4)
#add_definitions(-DC_COMP_MINOR_GCC_4_3)
#add_definitions(-DC_COMP_PATCH_GCC_4__2)

# Why is this a definition and not an option
add_definitions(-DWITH_OPENSSL)

add_definitions(-DCOMPILE_SOAP_SSL)

##################################################
# Now checking environment options


##################################################
# Now checking input options 
option(ENABLE_CHECKSUM_SHA1 "Enable production and validation of SHA1 checksums." OFF)
option(ENABLE_CHECKSUM_MD5 "Enable production and validation of MD5 checksums for released packages." OFF )
#this option will enable and disable other #defines.  
option(IS_CLIPPED ""


#option(HAS_FLOCK "" OFF)
#option(IS_CLIPPED "" )


# Find required dependencies.
# find_package(Boost REQUIRED thread)
# find_package(PythonLibs REQUIRED)
##################################################

message(STATUS "##########################################################################")
#CONFIGURE_FILE(${CONDOR_SOURCE_DIR}/includes/config.h.cmake ${CONDOR_SOURCE_DIR}/includes/config.h)
add_definitions(-DHAVE_CONFIG_H)

###########################################################################
# Include options used for custom macros which we wish to create
# list(APPEND CMAKE_MODULE_PATH ${CONDOR_SOURCE_DIR}/tools/cmake)

###########################################################################
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

# Set compiler flags
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /WX /wd4251 /wd4275 /wd4996")
else()
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread -g -Wall -W -Wextra -Wfloat-equal -Wshadow -Wendif-labels -Wpointer-arith -Wcast-qual -Wcast-align -Wvolatile-register-var -fstack-protector")

endif()

include_directories(${CONDOR_SOURCE_DIR}/include)
include_directories(${CONDOR_SOURCE_DIR}/libs/utils)

# Add components in dependency order
add_subdirectory(libs/utils)

