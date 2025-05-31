cmake_minimum_required(VERSION 3.13)
project(visualizer)

set(CMAKE_CXX_STANDARD 23)

if(WIN32)
    add_compile_options("/arch:AVX2")
    add_compile_options("/fp:fast")
endif()

# Use vendored dependencies
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
add_subdirectory(3rdparty/glfw)

add_subdirectory(3rdparty/glad_gen)

set(CATCH_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
set(CATCH_INSTALL_EXTRAS OFF CACHE BOOL "" FORCE)
add_subdirectory(3rdparty/Catch2)

# Add core utilities first as other modules depend on it
add_subdirectory(core)

add_subdirectory(audio_loopback)
add_subdirectory(audio_filters)
add_subdirectory(tests)
add_executable(visualizer main.cpp)
add_executable(glfwmaintest glfwmain.cpp)
target_link_libraries(glfwmaintest glfw glad::glad)
target_link_libraries(visualizer audio_loopback audio_filters glfw glad::glad core)