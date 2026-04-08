cmake_minimum_required(VERSION 3.10.0)
project(malloc VERSION 0.1.0 LANGUAGES C CXX)
if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
add_compile_options(-Wextra -Wall  -Wshadow -Wpointer-arith -Wfloat-equal -Wshadow)
add_compile_options(-Wcast-align -pthread -libm -ftls-model=initial-exec)
endif()

add_subdirectory(src)
