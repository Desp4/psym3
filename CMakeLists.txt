cmake_minimum_required(VERSION 3.17)

project(psym3)

file(GLOB_RECURSE PSYM3_SRC
    "${CMAKE_SOURCE_DIR}/src/*.h"
    "${CMAKE_SOURCE_DIR}/src/*.c")

add_executable(psym ${PSYM3_SRC})

if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set_target_properties(psym PROPERTIES COMPILE_FLAGS "/TC")
    add_compile_definitions("_CRT_SECURE_NO_WARNINGS")
endif()