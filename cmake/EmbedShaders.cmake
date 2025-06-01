# Function to embed shader files into a C++ header
function(embed_shaders TARGET HEADER_OUTPUT)
    set(SHADER_DIR "${CMAKE_SOURCE_DIR}/shaders")
    set(OUTPUT_FILE "${CMAKE_BINARY_DIR}/generated/${HEADER_OUTPUT}")
    
    # List of shader files to embed
    set(SHADER_FILES
        basic_vertex.glsl
        soundwave.glsl
        soundwave_optimized.glsl
        soundwave_ghost.glsl
        soundwave_trails.glsl
    )
    
    # Collect full paths
    set(SHADER_PATHS)
    foreach(SHADER ${SHADER_FILES})
        list(APPEND SHADER_PATHS "${SHADER_DIR}/${SHADER}")
    endforeach()
    
    # Create custom command to generate header
    add_custom_command(
        OUTPUT ${OUTPUT_FILE}
        COMMAND ${CMAKE_COMMAND}
            -DSHADER_DIR="${SHADER_DIR}"
            -DOUTPUT_FILE="${OUTPUT_FILE}"
            -P "${CMAKE_SOURCE_DIR}/cmake/GenerateShaderHeader.cmake"
        DEPENDS ${SHADER_PATHS}
        COMMENT "Generating embedded shader header"
    )
    
    # Create custom target
    add_custom_target(${TARGET}_shaders DEPENDS ${OUTPUT_FILE})
    
    # Add dependency and include path
    add_dependencies(${TARGET} ${TARGET}_shaders)
    target_include_directories(${TARGET} PRIVATE "${CMAKE_BINARY_DIR}/generated")
endfunction()