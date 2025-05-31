# Core utilities library
add_library(core INTERFACE)
target_include_directories(core INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
    $<INSTALL_INTERFACE:include>
)

# Set C++23 for the interface
target_compile_features(core INTERFACE cxx_std_23)