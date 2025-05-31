
if(WIN32)
    set(BACKEND "src/windows_backend.cc")
else()
    set(BACKEND "src/linux_backend.cc" "src/linux_audio_capture.cc")
endif()


add_library(audio_loopback
            ${BACKEND} src/ostream_operators.cpp)


target_include_directories(audio_loopback PUBLIC include)
target_link_libraries(audio_loopback PUBLIC core)

if(UNIX)
    target_link_libraries(audio_loopback PRIVATE pulse-simple)
endif()
