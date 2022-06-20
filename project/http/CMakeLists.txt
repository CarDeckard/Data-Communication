cmake_minimum_required(VERSION 3.17)
project(http C)

set(CMAKE_C_STANDARD 11)

if(MSVC)
    add_compile_options(/W4 /WX)
else()
    add_compile_options(-Wall -Wextra -pedantic)
endif()

add_executable(http http-server.c)