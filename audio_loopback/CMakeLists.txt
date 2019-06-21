add_library(audio_loopback
        src/windows_backend.cc src/ostream_operators.cpp)

target_include_directories(audio_loopback PUBLIC include)
