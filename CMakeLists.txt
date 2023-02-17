cmake_minimum_required(VERSION 3.22)
project(chungus)

# chungs
add_executable(chungus test.cpp)

# env
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set_property(TARGET chungus PROPERTY CXX_STANDARD 20)

# opengl
find_package(OpenGL REQUIRED)

include_directories(${OPENGL_INCLUDE_DIRS})
link_directories(${OPENGL_LIBRARY_DIR})
target_link_libraries(chungus ${OPENGL_LIBRARIES})

# glfw
set(GLFW_BUILD_DOCS OFF CACHE BOOL "")
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "")

add_subdirectory(external/glfw)
# find_package(glfw REQUIRED)

include_directories(${GLFW_INCLUDE_DIRS})
link_directories(${GLFW_LIBRARY_DIR})
target_link_libraries(chungus glfw)

# glm
add_subdirectory(external/glm)
# find_package(glm REQUIRED)

include_directories(${glm_INCLUDE_DIRS})
link_directories(${glm_LIBRARY_DIR})
target_link_libraries(chungus glm)

# boost
set(Boost_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/external/boost/)
set(Boost_LIBRARY_DIR ${CMAKE_SOURCE_DIR}/external/boost/stage/lib/)

add_subdirectory(external/boost)
find_package(Boost REQUIRED thread)

include_directories(${Boost_INCLUDE_DIR})
link_directories(${Boost_LIBRARY_DIR})
target_link_libraries(chungus ${Boost_LIBRARIES})
