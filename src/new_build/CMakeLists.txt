# Set project name
project(CONDOR)

# Require at least CMake version 2.6
cmake_minimum_required(VERSION 2.6)

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


# On Windows the Boost libraries are linked automatically
#if(MSVC)
#    set(Boost_LIBRARIES "")
#endif()

# Set compiler flags
if(MSVC)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /WX /wd4251 /wd4275 /wd4996")
else()
    add_definitions(-D_LINUX)
endif()

include_directories(${CONDOR_SOURCE_DIR}/condor_includes)

# Add components in dependency order
#add_subdirectory(condor_daemon_core.V6)
add_subdirectory(condor_c++_util)

