# Use Catch2 from vcpkg
add_executable(tests 
    test_main.cpp
    # test_fft.cpp  # Disabled for now
    test_core.cpp
)

target_link_libraries(tests PRIVATE Catch2::Catch2WithMain)
target_include_directories(tests PRIVATE ../core)