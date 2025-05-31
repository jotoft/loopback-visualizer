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
add_subdirectory(tests)
add_executable(visualizer main.cpp)
add_executable(visualizer_optimized main_optimized.cpp)
add_executable(visualizer_gui main_gui.cpp)
add_executable(visualizer_gui_improved main_gui_improved.cpp)
add_executable(glfwmaintest glfwmain.cpp)
add_executable(audio_example audio_example.cpp)
target_link_libraries(glfwmaintest glfw glad::glad)
target_link_libraries(visualizer audio_loopback audio_filters glfw glad::glad core imgui)
target_link_libraries(visualizer_optimized audio_loopback audio_filters glfw glad::glad core imgui)
target_link_libraries(visualizer_gui audio_loopback audio_filters glfw glad::glad core imgui)
target_link_libraries(visualizer_gui_improved audio_loopback audio_filters glfw glad::glad core imgui)
target_link_libraries(audio_example audio_loopback core)

# Copy shader files to build directory whenever they change
add_custom_command(
    TARGET visualizer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/soundwave.glsl
        ${CMAKE_SOURCE_DIR}/basic_vertex.glsl
        $<TARGET_FILE_DIR:visualizer>
    COMMENT "Copying shader files to build directory"
)

add_custom_command(
    TARGET visualizer_optimized POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/soundwave.glsl
        ${CMAKE_SOURCE_DIR}/soundwave_optimized.glsl
        ${CMAKE_SOURCE_DIR}/basic_vertex.glsl
        $<TARGET_FILE_DIR:visualizer_optimized>
    COMMENT "Copying shader files to build directory"
)

add_custom_command(
    TARGET visualizer_gui POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/soundwave.glsl
        ${CMAKE_SOURCE_DIR}/soundwave_optimized.glsl
        ${CMAKE_SOURCE_DIR}/basic_vertex.glsl
        $<TARGET_FILE_DIR:visualizer_gui>
    COMMENT "Copying shader files to build directory"
)

add_custom_command(
    TARGET visualizer_gui_improved POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/soundwave.glsl
        ${CMAKE_SOURCE_DIR}/soundwave_optimized.glsl
        ${CMAKE_SOURCE_DIR}/basic_vertex.glsl
        $<TARGET_FILE_DIR:visualizer_gui_improved>
    COMMENT "Copying shader files to build directory"
)