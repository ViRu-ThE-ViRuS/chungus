cmake_minimum_required(VERSION 3.22)
project(glad)

set(glad_INCLUDE_DIRS ${CMAKE_SOURCE_DIR}/include)

add_library(glad include/glad/vulkan.h src/vulkan.c)
include_directories(${glad_INCLUDE_DIRS})
target_include_directories(glad PUBLIC include/)
