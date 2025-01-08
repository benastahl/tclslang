cmake_minimum_required(VERSION 3.15)

# Project name
project(tclslang)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Find slang library
find_package(slang REQUIRED)

# Use pkg-config to find Tcl
find_package(PkgConfig REQUIRED)
pkg_check_modules(TCL REQUIRED tcl)

# Find fmt library
find_package(fmt REQUIRED)

# Add the shared library target
add_library(tclslang SHARED tclslang.cpp)

# Ensure position-independent code (for shared libraries)
set_target_properties(tclslang PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Link the shared library with its dependencies
target_link_libraries(tclslang
        PUBLIC fmt::fmt
        PUBLIC slang::slang ${TCL_LIBRARIES}
)

# Include Tcl headers
target_include_directories(tclslang PUBLIC ${TCL_INCLUDE_DIRS})
