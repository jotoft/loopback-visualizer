# Use vendored Catch2
add_executable(tests 
    test_main.cpp
    # test_fft.cpp  # Disabled for now
    test_core.cpp
    test_audio.cpp
    test_audio_ring_buffer.cpp
)

target_link_libraries(tests PRIVATE Catch2::Catch2WithMain core audio_loopback)

# Register tests with CTest
enable_testing()
add_test(NAME core_tests COMMAND tests)