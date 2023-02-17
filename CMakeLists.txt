cmake_minimum_required(VERSION 3.22)
project(chungus)

# chungs
file(GLOB_RECURSE SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/src/*.c
    ${CMAKE_SOURCE_DIR}/src/*.cpp)

file(GLOB_RECURSE HEADER_FILES
    ${CMAKE_SOURCE_DIR}/src/*.h
    ${CMAKE_SOURCE_DIR}/src/*.hpp)

add_executable(chungus ${SOURCE_FILES} ${HEADER_FILES})

# env
set(VENDOR_DIR ${CMAKE_SOURCE_DIR}/external)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set_property(TARGET chungus PROPERTY CXX_STANDARD 20)
target_compile_options(chungus PRIVATE -Wall)

# glfw
set(GLFW_BUILD_DOCS OFF CACHE BOOL "")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "")

set(GLFW_INCLUDE_DIRS ${VENDOR_DIR}/glfw/include/)
add_subdirectory(${VENDOR_DIR}/glfw)
include_directories(${GLFW_INCLUDE_DIRS})
target_link_libraries(chungus glfw ${GLFW_LIBRARIES})

# glm
set(GLM_INCLUDE_DIRS ${VENDOR_DIR}/glm/)
add_subdirectory(${VENDOR_DIR}/glm)
include_directories(${GLM_INCLUDE_DIRS})

# boost
set(Boost_INCLUDE_DIR ${VENDOR_DIR}/boost/)
set(Boost_LIBRARY_DIR ${VENDOR_DIR}/boost/stage/lib/)
set(Boost_NO_WARN_NEW_VERSIONS ON)

add_subdirectory(${VENDOR_DIR}/boost)
find_package(Boost REQUIRED thread)

include_directories(${Boost_INCLUDE_DIR})
link_directories(${Boost_LIBRARY_DIR})
target_link_libraries(chungus ${Boost_LIBRARIES})
