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

# Add ImGui
add_subdirectory(3rdparty/imgui)

# Add core utilities first as other modules depend on it
add_subdirectory(core)

add_subdirectory(audio_loopback)
add_subdirectory(audio_filters)
add_subdirectory(src)
add_subdirectory(tests)
add_executable(visualizer src/main.cpp)
add_executable(audio_example audio_example.cpp)
target_link_libraries(visualizer audio_loopback audio_filters visualization glfw glad::glad core imgui)
target_link_libraries(audio_example audio_loopback core)



# Copy shader files to build directory whenever they change
add_custom_command(
    TARGET visualizer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/shaders/soundwave.glsl
        ${CMAKE_SOURCE_DIR}/shaders/soundwave_optimized.glsl
        ${CMAKE_SOURCE_DIR}/shaders/basic_vertex.glsl
        $<TARGET_FILE_DIR:visualizer>
    COMMENT "Copying shader files to build directory"
)