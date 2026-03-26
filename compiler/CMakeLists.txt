cmake_minimum_required(VERSION 3.10)
project(UrusCompiler VERSION 0.3.0 LANGUAGES C)

set(PROJECT_VERSION_TAGS "")
set(CMAKE_C_STANDARD 11) # -std=c11
set(CMAKE_C_STANDARD_REQUIRED ON)

configure_file(include/config.h.in config.h)

if(MSVC)
    add_compile_options(/W4)
    add_compile_definitions(_CRT_SECURE_NO_WARNINGS _CRT_NONSTDC_NO_DEPRECATE)
else()
    add_compile_options(-Wall -Wextra -g)
endif()

set(URUS_RUNTIME_HEADER "${CMAKE_CURRENT_SOURCE_DIR}/runtime/urus_runtime.h")
set(URUS_RUNTIME_EMBEDDED "${CMAKE_CURRENT_BINARY_DIR}/urus_runtime.c")

add_custom_command(
    OUTPUT "${URUS_RUNTIME_EMBEDDED}"
    COMMAND ${CMAKE_COMMAND}
            -DINPUT_FILE=${URUS_RUNTIME_HEADER}
            -DOUTPUT_FILE=${URUS_RUNTIME_EMBEDDED}
            -DVARIABLE_NAME=urus_runtime_header_data
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed-string.cmake"
    DEPENDS "${URUS_RUNTIME_H}" "${CMAKE_CURRENT_SOURCE_DIR}/cmake/embed-string.cmake"
)

# source
set(SOURCES
    src/main.c
    src/lexer.c
    src/ast.c
    src/parser.c
    src/util.c
    src/error.c
    src/Sema/sema.c
    src/Sema/scope.c
    src/Sema/builtins.c
    src/codegen.c
    src/preprocess.c
)

add_executable(urusc ${SOURCES} ${URUS_RUNTIME_EMBEDDED})

target_include_directories(urusc PRIVATE "${CMAKE_CURRENT_BINARY_DIR}") # -I<BUILD DIRECTORIES> (example for config.h by config.h.in)
target_include_directories(urusc PRIVATE include) # -Iinclude

# -lm for non-windows
if(UNIX AND NOT APPLE)
    target_link_libraries(urusc PRIVATE m)
endif()

# Install urusc
install(TARGETS urusc DESTINATION bin)

# Install stdlib/* to local lib
if(WIN32)
    # Windows: Program Files\Urusc\lib
    # Path separator already handled by CMake
    set(URUS_LIB_DIR "Program Files/Urusc/lib")
elseif(DEFINED ENV{PREFIX})
    # Termux sets $PREFIX; honour it.
    set(URUS_LIB_DIR "$ENV{PREFIX}/lib/urusc")
else()
    # Linux/Unix: /usr/local/lib/urusc
    set(URUS_LIB_DIR "/usr/local/lib/urusc")
endif()

install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/stdlib/"
        DESTINATION "${URUS_LIB_DIR}"
        FILES_MATCHING PATTERN "*.urus")

# CTest integration
enable_testing()

set(TESTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../tests")

# Valid compilation tests (should pass)
file(GLOB VALID_TESTS "${TESTS_DIR}/valid/*.urus")
foreach(test_file ${VALID_TESTS})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_test(NAME valid_${test_name} COMMAND urusc ${test_file} --emit-c)
endforeach()

# Invalid compilation tests (should fail)
file(GLOB INVALID_TESTS "${TESTS_DIR}/invalid/*.urus")
foreach(test_file ${INVALID_TESTS})
    get_filename_component(test_name ${test_file} NAME_WE)
    add_test(NAME invalid_${test_name} COMMAND urusc ${test_file} --emit-c)
    set_tests_properties(invalid_${test_name} PROPERTIES WILL_FAIL TRUE)
endforeach()

# Run tests (compile, run, check output)
file(GLOB RUN_TESTS "${TESTS_DIR}/run/*.urus")
foreach(test_file ${RUN_TESTS})
    get_filename_component(test_name ${test_file} NAME_WE)
    set(expected_file "${TESTS_DIR}/run/expected/${test_name}.expected")

    if(EXISTS ${expected_file})
        add_test(NAME run_${test_name}
            COMMAND ${CMAKE_COMMAND}
                -DURUSC=$<TARGET_FILE:urusc>
                -DFILE=${test_file}
                -DEXPECTED=${expected_file}
                -DINCLUDE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/include
                -DWORKING_DIR=${CMAKE_CURRENT_BINARY_DIR}
                -DCMAKE_C_COMPILER=${CMAKE_C_COMPILER}
                -P "${TESTS_DIR}/run_tests.cmake"
        )
    endif()
endforeach()
