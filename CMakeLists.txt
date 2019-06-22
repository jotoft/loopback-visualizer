cmake_minimum_required(VERSION 3.13)
project(visualizer)

set(CMAKE_CXX_STANDARD 14)

add_subdirectory(3rdparty/glad)
add_subdirectory(3rdparty/glfw)
add_subdirectory(audio_loopback)
add_executable(visualizer main.cpp)
add_executable(glfwmaintest glfwmain.cpp)
target_link_libraries(glfwmaintest glfw glad)
target_link_libraries(visualizer audio_loopback glfw glad)