cmake_minimum_required(VERSION 3.22)
project(chungus)

# chungus
file(GLOB_RECURSE SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/src/*.c
    ${CMAKE_SOURCE_DIR}/src/*.cpp)

file(GLOB_RECURSE HEADER_FILES
    ${CMAKE_SOURCE_DIR}/src/*.h
    ${CMAKE_SOURCE_DIR}/src/*.hpp)

add_executable(chungus ${SOURCE_FILES} ${HEADER_FILES})
include_directories(${CMAKE_SOURCE_DIR}/src)

set(VENDOR_DIR ${CMAKE_SOURCE_DIR}/external)
set(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH} ${VENDOR_DIR})
set(SHADER_SRC_DIR ${CMAKE_SOURCE_DIR}/src/shaders)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set_property(TARGET chungus PROPERTY CXX_STANDARD 20)

# compile and copy shaders to build dir on make
add_custom_command(
  TARGET chungus POST_BUILD
  COMMAND mkdir -p ${CMAKE_CURRENT_BINARY_DIR}/shaders

  # compile default shaders into build dir
  COMMAND glslangValidator -V ${SHADER_SRC_DIR}/default.frag -o $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders/default.frag.spv
  COMMAND glslangValidator -V ${SHADER_SRC_DIR}/default.vert -o $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders/default.vert.spv)

# vulkan
find_package(Vulkan REQUIRED)
include_directories(${Vulkan_INCLUDE_DIRS})
target_link_libraries(chungus ${Vulkan_LIBRARIES})

# GSL
set(GSL_INCLUDE_DIRS ${VENDOR_DIR}/GSL/include)
add_subdirectory(${VENDOR_DIR}/GSL)
include_directories(${GSL_INCLUDE_DIRS})

# glfw
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

set(GLFW_INCLUDE_DIRS ${VENDOR_DIR}/glfw/include/)
add_subdirectory(${VENDOR_DIR}/glfw)
include_directories(${GLFW_INCLUDE_DIRS})
target_link_libraries(chungus glfw)

# glm
set(GLM_INCLUDE_DIRS ${VENDOR_DIR}/glm)
add_subdirectory(${VENDOR_DIR}/glm)
include_directories(${GLM_INCLUDE_DIRS})

