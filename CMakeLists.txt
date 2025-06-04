# Minimum CMake version
cmake_minimum_required(VERSION 3.15)

# Project name
project(tclslang)

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Find external dependencies
find_package(slang REQUIRED)     # SystemVerilog compiler library
find_package(PkgConfig REQUIRED) # To use pkg-config
pkg_check_modules(TCL REQUIRED tcl) # Find the Tcl library
find_package(fmt REQUIRED)       # Fast formatting library

# Automatically find all source and header files
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS src/*.cpp)
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS include/*.h)

# Add the shared library target
add_library(tclslang SHARED
        ${SOURCES}
        ${HEADERS}
        include/tclslang/hdl_tree.hpp   # (explicitly listed header for safety)
)

# Ensure position-independent code (important for shared libraries)
set_target_properties(tclslang PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Link the shared library with its dependencies
target_link_libraries(tclslang
        PUBLIC fmt::fmt             # Link to fmt formatting library
        PUBLIC slang::slang         # Link to slang SystemVerilog library
        ${TCL_LIBRARIES}            # Link to Tcl libraries
)

# Add include directories so your project knows where to find headers
target_include_directories(tclslang
        PUBLIC
        ${TCL_INCLUDE_DIRS}          # Include Tcl headers
        ${CMAKE_CURRENT_SOURCE_DIR}/include  # Include your own headers
)
