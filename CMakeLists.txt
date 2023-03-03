cmake_minimum_required(VERSION 3.24)
project(FileManager C)

set(CMAKE_C_STANDARD 11)

add_compile_options(-W -Wall -Werror -Wextra -lncurses)

add_executable(FileManager main.c)