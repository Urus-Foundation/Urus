# Compiler core sources, shared between the CLI and the fuzz target.
set(URUS_CORE_SOURCES
    src/diag.c
    src/lexer.c
    src/parser.c
    src/ast.c
    src/sema.c
    src/codegen_c.c
    src/strbuf.c
    src/arena.c
    src/compile.c
    src/urus_abort.c
)

add_library(urus_core STATIC ${URUS_CORE_SOURCES})
target_include_directories(urus_core PUBLIC include)

if(WIN32)
    target_compile_definitions(urus_core PUBLIC URUS_PLATFORM_WINDOWS=1)
elseif(APPLE)
    target_compile_definitions(urus_core PUBLIC URUS_PLATFORM_MACOS=1)
else()
    target_compile_definitions(urus_core PUBLIC URUS_PLATFORM_LINUX=1)
endif()

add_executable(urusc src/main.c)
target_link_libraries(urusc PRIVATE urus_core)

set_target_properties(urusc PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# libFuzzer target (clang only) — opt in with -DURUS_BUILD_FUZZER=ON.
option(URUS_BUILD_FUZZER "Build the libFuzzer target (requires clang)" OFF)
if(URUS_BUILD_FUZZER)
    if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "URUS_BUILD_FUZZER requires clang (got ${CMAKE_C_COMPILER_ID})")
    endif()
    add_executable(fuzz_compile ${CMAKE_SOURCE_DIR}/fuzz/fuzz_compile.c)
    target_link_libraries(fuzz_compile PRIVATE urus_core)
    target_compile_options(fuzz_compile PRIVATE -fsanitize=fuzzer,address,undefined)
    target_link_options(fuzz_compile PRIVATE -fsanitize=fuzzer,address,undefined)
    # The core library must also be sanitized for instrumentation coverage.
    target_compile_options(urus_core PRIVATE -fsanitize=fuzzer-no-link,address,undefined)
    # urusc links the same (now-instrumented) core, so it needs the
    # sanitizer runtimes too or its link fails with undefined __asan_* /
    # __sancov_* references.
    target_link_options(urusc PRIVATE -fsanitize=address,undefined)
endif()
