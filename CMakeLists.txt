cmake_minimum_required(VERSION 3.13)
project(visualizer)

set(CMAKE_CXX_STANDARD 23)

if(WIN32)
    add_compile_options("/arch:AVX2")
    add_compile_options("/fp:fast")
endif()

# Disable GLFW docs/examples/tests to speed up builds
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)

# Disable Catch2 tests/examples to speed up builds
set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
set(CATCH_DEVELOPMENT_BUILD OFF CACHE BOOL "" FORCE)
set(CATCH_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(CATCH_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CATCH_BUILD_EXTRA_TESTS OFF CACHE BOOL "" FORCE)

add_subdirectory(3rdparty/glad/cmake)
add_subdirectory(3rdparty/glfw)
add_subdirectory(3rdparty/Catch2)

# Configure GLAD 2.0 - generate OpenGL 3.3 core profile loader
glad_add_library(glad_gl_core_33 STATIC API gl:core=3.3)
add_subdirectory(audio_loopback)
add_subdirectory(audio_filters)
add_subdirectory(tests)
add_executable(visualizer main.cpp)
add_executable(glfwmaintest glfwmain.cpp)
target_link_libraries(glfwmaintest glfw glad_gl_core_33)
target_link_libraries(visualizer audio_loopback audio_filters glfw glad_gl_core_33)