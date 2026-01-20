cmake_minimum_required(VERSION 3.13)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project("DSRT" VERSION 1.0.0)

set(MANDRILL_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

find_package(Vulkan REQUIRED COMPONENTS glslc)
include_directories(${Vulkan_INCLUDE_DIRS})

set(MANDRILL_EXCLUDE_APPS ON)
include_directories(Mandrill/src)
include_directories(Mandrill/src/External)
include_directories(Mandrill/submodules/glm)
include_directories(src)

add_subdirectory(Mandrill)
add_subdirectory(src)