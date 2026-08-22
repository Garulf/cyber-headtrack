cmake_minimum_required(VERSION 3.24)
project(CyberHeadtrack LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(CHT_BUILD_PLUGIN "Build the Windows-only RED4ext plugin target" OFF)

add_subdirectory(src/portable)

enable_testing()
add_subdirectory(tests)

if(CHT_BUILD_PLUGIN)
    add_subdirectory(src/plugin)
endif()
