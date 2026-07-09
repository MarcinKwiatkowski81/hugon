cmake_minimum_required(VERSION 3.16)

# ── test_minijson ─────────────────────────────────────────────────────────────
# Desktop build of the MiniJson test suite.
# The library itself is header-only; we only need a C++ compiler.

add_executable(test_minijson
    test_minijson.cpp
)

target_include_directories(test_minijson PRIVATE
    # Resolve #include "../../src/MiniJson.h" from the test file's location.
    # No additional include path is needed because the relative path in the
    # #include directive covers it, but we also expose the src/ directory so
    # users can write   #include <MiniJson.h>   in their own code.
    "${CMAKE_CURRENT_SOURCE_DIR}/../../src"
)

target_compile_options(test_minijson PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

set_target_properties(test_minijson PROPERTIES
    CXX_STANDARD          11   # Arduino Uno toolchain supports C++11
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS        OFF
)

add_test(
    NAME    test_minijson
    COMMAND test_minijson
)

# ── test_minijson_nofloat ─────────────────────────────────────────────────────
# Same test binary compiled with -DMINIJSON_NO_FLOAT.
# Fractional numbers must now come back as EV_INTEGER, not EV_FLOAT.
# We compile a separate binary that runs the subset of tests that care about
# this flag (all other tests remain unaffected because they only check
# EV_INTEGER / EV_BOOL / EV_NULL / EV_STRING).

add_executable(test_minijson_nofloat
    test_minijson.cpp
)

target_include_directories(test_minijson_nofloat PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../../src"
)

target_compile_definitions(test_minijson_nofloat PRIVATE
    MINIJSON_NO_FLOAT
)

target_compile_options(test_minijson_nofloat PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

set_target_properties(test_minijson_nofloat PROPERTIES
    CXX_STANDARD          11
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS        OFF
)

add_test(
    NAME    test_minijson_nofloat
    COMMAND test_minijson_nofloat
)
