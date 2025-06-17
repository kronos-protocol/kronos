cmake_minimum_required(VERSION 3.31)
project(kronos C)

set(CMAKE_C_STANDARD 11)

add_library(kronos STATIC library.c)
