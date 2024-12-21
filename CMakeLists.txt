cmake_minimum_required(VERSION 3.29)
project(os_lab2 C)

set(CMAKE_C_STANDARD 11)

include_directories(include)

file(GLOB_RECURSE SOURCES "src/*.c")
file(GLOB_RECURSE HEADERS "include/*.h")

add_executable(os_lab2 main.c ${SOURCES} ${HEADERS})
