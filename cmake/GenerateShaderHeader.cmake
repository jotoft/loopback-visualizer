# Script to generate shader header file
# Called by EmbedShaders.cmake

file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/generated")

set(HEADER_CONTENT "#pragma once

#include <string_view>

namespace shaders {

")

# Function to convert filename to variable name
function(get_shader_var_name FILENAME VAR_NAME)
    string(REPLACE ".glsl" "" BASE_NAME "${FILENAME}")
    string(REPLACE "-" "_" BASE_NAME "${BASE_NAME}")
    set(${VAR_NAME} "${BASE_NAME}_shader" PARENT_SCOPE)
endfunction()

# Process each shader file
file(GLOB SHADER_FILES "${SHADER_DIR}/*.glsl")
foreach(SHADER_PATH ${SHADER_FILES})
    get_filename_component(SHADER_NAME "${SHADER_PATH}" NAME)
    get_shader_var_name("${SHADER_NAME}" VAR_NAME)
    
    # Read shader content
    file(READ "${SHADER_PATH}" SHADER_CONTENT)
    
    # Escape any raw string literal endings
    string(REPLACE ")glsl\"" ") glsl\"" SHADER_CONTENT "${SHADER_CONTENT}")
    
    # Add to header
    set(HEADER_CONTENT "${HEADER_CONTENT}// Source: ${SHADER_NAME}
//language=glsl
constexpr std::string_view ${VAR_NAME} = R\"glsl(
${SHADER_CONTENT})glsl\";

")
endforeach()

# Close namespace
set(HEADER_CONTENT "${HEADER_CONTENT}} // namespace shaders
")

# Write the header file
file(WRITE "${OUTPUT_FILE}" "${HEADER_CONTENT}")

message(STATUS "Generated shader header: ${OUTPUT_FILE}")