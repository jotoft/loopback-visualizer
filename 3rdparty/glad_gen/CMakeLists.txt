cmake_minimum_required(VERSION 3.13)
project(glad)

# Create glad library with pre-generated sources
add_library(glad STATIC
    src/gl.c
    include/glad/gl.h
    include/KHR/khrplatform.h
)

target_include_directories(glad PUBLIC 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Create alias for compatibility
add_library(glad::glad ALIAS glad)