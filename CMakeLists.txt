cmake_minimum_required(VERSION 3.16)
project(hugon VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ── Library ────────────────────────────────────────────────────────────────────

add_library(hugon
    src/Hugon.cpp
    src/DJSON.cpp
)
add_library(hugon::hugon ALIAS hugon)

target_include_directories(hugon
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_compile_options(hugon PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# ── Install ────────────────────────────────────────────────────────────────────

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

install(TARGETS hugon
    EXPORT hugonTargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})

install(EXPORT hugonTargets
    FILE       hugonTargets.cmake
    NAMESPACE  hugon::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hugon
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/hugonConfigVersion.cmake"
    VERSION       ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/hugonConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/hugonConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hugon
)

install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/hugonConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/hugonConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/hugon
)

# ── Tests ──────────────────────────────────────────────────────────────────────

option(HUGON_BUILD_TESTS    "Build test suite"  ON)
option(HUGON_BUILD_EXAMPLES "Build examples"    ON)

if(HUGON_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(HUGON_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()
