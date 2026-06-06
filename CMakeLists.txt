cmake_minimum_required(VERSION 3.16)
project(urus C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug)
endif()

if(MSVC)
    add_compile_options(/W4 /wd4100 /wd4244 /wd4267 /wd4996)
else()
    add_compile_options(-Wall -Wextra -Wno-unused-parameter -Wno-unused-function)
endif()

add_subdirectory(compiler)

enable_testing()
if(EXISTS "${CMAKE_SOURCE_DIR}/tests/CMakeLists.txt")
    add_subdirectory(tests)
endif()
