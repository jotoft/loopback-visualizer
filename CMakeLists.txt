cmake_minimum_required(VERSION 3.13)
project(visualizer)

set(CMAKE_CXX_STANDARD 23)

if(WIN32)
    add_compile_options("/arch:AVX2")
    add_compile_options("/fp:fast")
endif()

# Find packages installed via vcpkg
find_package(glad CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(Catch2 3 CONFIG REQUIRED)
add_subdirectory(audio_loopback)
add_subdirectory(audio_filters)
add_subdirectory(tests)
add_executable(visualizer main.cpp)
add_executable(glfwmaintest glfwmain.cpp)
target_link_libraries(glfwmaintest glfw glad::glad)
target_link_libraries(visualizer audio_loopback audio_filters glfw glad::glad)