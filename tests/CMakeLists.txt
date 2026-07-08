add_executable(test_hugon test_hugon.cpp)

target_link_libraries(test_hugon PRIVATE hugon::hugon)

target_compile_options(test_hugon PRIVATE
    $<$<CXX_COMPILER_ID:GNU,Clang>:-Wall -Wextra>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# Copy test data next to the built executable so the test can open it by name.
configure_file(schemaBig.json schemaBig.json COPYONLY)

add_test(
    NAME    test_hugon
    COMMAND test_hugon
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)
