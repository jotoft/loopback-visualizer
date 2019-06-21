cmake_minimum_required(VERSION 3.13)
project(visualizer)

set(CMAKE_CXX_STANDARD 14)

add_subdirectory(audio_loopback)
add_executable(visualizer main.cpp)
target_link_libraries(visualizer audio_loopback)