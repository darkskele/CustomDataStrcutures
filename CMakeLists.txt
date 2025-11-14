cmake_minimum_required(VERSION 3.20)

project(mcds
    VERSION 0.1.0
    LANGUAGES CXX
)

# Global C++ config
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Header-only library
add_library(mcds INTERFACE)

target_include_directories(mcds
    INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Tests 
enable_testing()

# GTest via package config
find_package(GTest CONFIG REQUIRED)

add_subdirectory(tests)


add_subdirectory(benchmarks)
