# Visualization library
add_library(visualization STATIC
    phase_lock_analyzer.cpp
)

target_include_directories(visualization PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_compile_features(visualization PUBLIC cxx_std_23)