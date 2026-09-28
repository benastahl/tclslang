# Minimum CMake version
cmake_minimum_required(VERSION 3.20)

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

# AddressSanitizer + UndefinedBehaviorSanitizer build of the extension.
# tclsh itself is not instrumented, so the ASan runtime is preloaded into the
# test processes (see TCLSLANG_TEST_ENV below).
#
# Use Release or RelWithDebInfo, matching how slang was built: slang's headers
# change class layouts when NDEBUG is unset (SLANG_ASSERT_ENABLED), so a Debug
# build against a Release slang crashes inside slang.
option(TCLSLANG_SANITIZE "Build with -fsanitize=address,undefined" OFF)
set(TCLSLANG_TEST_ENV "")
if(TCLSLANG_SANITIZE)
    set(SAN_FLAGS -fsanitize=address,undefined -fno-sanitize-recover=undefined
                  -fno-omit-frame-pointer)
    target_compile_options(tclslang PRIVATE ${SAN_FLAGS})
    target_link_options(tclslang PRIVATE ${SAN_FLAGS})
    execute_process(COMMAND ${CMAKE_CXX_COMPILER} -print-file-name=libasan.so
                    OUTPUT_VARIABLE ASAN_RUNTIME OUTPUT_STRIP_TRAILING_WHITESPACE)
    set(TCLSLANG_TEST_ENV
        "LD_PRELOAD=${ASAN_RUNTIME}"
        "ASAN_OPTIONS=detect_leaks=0"
        "UBSAN_OPTIONS=print_stacktrace=1")
endif()

# ---------------------------------------------------------------------------
# Tests: one golden-output test per case in tests/cases, plus the tcltest
# API suite. Run with `ctest --test-dir build --output-on-failure`.
# ---------------------------------------------------------------------------
option(TCLSLANG_BUILD_TESTS "Register the regression tests with CTest" ON)

if(TCLSLANG_BUILD_TESTS)
    enable_testing()
    find_program(TCLSH NAMES tclsh tclsh8.6 REQUIRED)

    set(TEST_DIR ${CMAKE_CURRENT_SOURCE_DIR}/tests)

    # Cases the library is known to get wrong. They stay registered (and
    # visible as "Not Run (Disabled)") until the fix lands with its golden.
    set(TCLSLANG_KNOWN_FAILURES
        # (none)
    )

    file(GLOB CASES CONFIGURE_DEPENDS ${TEST_DIR}/cases/*)
    foreach(case ${CASES})
        if(case MATCHES "\\.golden$")
            continue()
        endif()
        get_filename_component(name ${case} NAME_WE)
        add_test(NAME golden.${name}
                 COMMAND ${TCLSH} ${TEST_DIR}/run_golden.tcl
                         $<TARGET_FILE:tclslang> ${case}
                         ${CMAKE_CURRENT_BINARY_DIR}/golden-actual)
        set_tests_properties(golden.${name} PROPERTIES
                             ENVIRONMENT "${TCLSLANG_TEST_ENV}")
        if(name IN_LIST TCLSLANG_KNOWN_FAILURES)
            set_tests_properties(golden.${name} PROPERTIES DISABLED TRUE)
        endif()
    endforeach()

    add_test(NAME api
             COMMAND ${TCLSH} ${TEST_DIR}/api.test $<TARGET_FILE:tclslang>)
    set_tests_properties(api PROPERTIES ENVIRONMENT "${TCLSLANG_TEST_ENV}")
endif()
