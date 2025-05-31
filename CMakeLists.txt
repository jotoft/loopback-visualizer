cmake_minimum_required(VERSION 3.13)
project(visualizer)

set(CMAKE_CXX_STANDARD 23)

if(WIN32)
    add_compile_options("/arch:AVX2")
    add_compile_options("/fp:fast")
endif()

add_subdirectory(3rdparty/glad)
add_subdirectory(3rdparty/glfw)
add_subdirectory(audio_loopback)
add_subdirectory(audio_filters)
add_subdirectory(tests)
add_executable(visualizer main.cpp)
add_executable(glfwmaintest glfwmain.cpp)
target_link_libraries(glfwmaintest glfw glad)
target_link_libraries(visualizer audio_loopback audio_filters glfw glad)