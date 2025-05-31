add_executable(tests 
    test_main.cpp
    test_fft.cpp
)

# Add include directory for catch.hpp
target_include_directories(tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})