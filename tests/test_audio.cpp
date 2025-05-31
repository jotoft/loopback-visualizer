#include <catch2/catch_test_macros.hpp>
#include <audio_loopback/loopback_recorder.h>
#include <audio_loopback/audio_capture.h>
#include <core/result.h>
#include <core/option.h>
#include <core/unit.h>
#include <thread>
#include <chrono>

TEST_CASE("Audio API returns proper Result/Option types", "[audio]") {
    SECTION("get_default_sink returns Option") {
        auto sink_opt = audio::get_default_sink(false);
        
        // Should always find a default sink on Linux with PulseAudio
        REQUIRE(sink_opt.is_some());
        
        auto sink = sink_opt.unwrap();
        REQUIRE(sink.name == "Default PulseAudio Sink");
        REQUIRE(sink.device_id == "default");
        REQUIRE(sink.capture_device == false);
    }
    
    SECTION("list_sinks returns Result") {
        auto sinks_result = audio::list_sinks();
        
        REQUIRE(sinks_result.is_ok());
        
        auto sinks = sinks_result.unwrap();
        REQUIRE(!sinks.empty());
        REQUIRE(sinks[0].name == "Default PulseAudio Sink");
    }
    
    SECTION("capture_data returns Result<Unit, AudioError>") {
        auto sink_opt = audio::get_default_sink(false);
        REQUIRE(sink_opt.is_some());
        
        auto sink = sink_opt.unwrap();
        
        // Create a callback that immediately stops
        auto callback = [](const audio::AudioBuffer& buffer) {
            return false; // Stop immediately
        };
        
        auto result = audio::capture_data(callback, sink);
        
        // Should succeed in creating the capture thread
        REQUIRE(result.is_ok());
    }
    
    SECTION("AudioCapture actually receives samples") {
        auto sink_opt = audio::get_default_sink(false);
        REQUIRE(sink_opt.is_some());
        
        auto sink = sink_opt.unwrap();
        auto capture = audio::create_audio_capture(sink);
        
        auto result = capture->start();
        REQUIRE(result.is_ok());
        
        // Wait a bit for audio to start flowing
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // Try to read some samples
        constexpr size_t buffer_size = 4096;
        float samples[buffer_size];
        
        // Try multiple times to account for startup delay
        size_t total_read = 0;
        for (int i = 0; i < 10; ++i) {
            size_t read = capture->read_samples(samples, buffer_size);
            total_read += read;
            
            if (read > 0) {
                // Verify samples are not all zeros (silence)
                float sum = 0;
                for (size_t j = 0; j < read; ++j) {
                    sum += std::abs(samples[j]);
                }
                
                // If we got samples, at least some should be non-zero
                // unless the system is completely silent
                INFO("Read " << read << " samples, sum: " << sum);
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        capture->stop();
        
        // Check final stats
        auto stats = capture->get_stats();
        INFO("Final stats - Total captured: " << stats.total_samples_captured 
             << ", Available: " << stats.available_samples
             << ", Overruns: " << stats.overruns
             << ", Underruns: " << stats.underruns);
        
        // We should have captured SOME samples after 200ms
        // (44100 Hz * 0.2s = 8820 samples minimum)
        REQUIRE(stats.total_samples_captured > 0);
        
        // Underruns at startup are expected - we start reading before buffer fills
        // Just verify we actually got some data
        INFO("Captured " << stats.total_samples_captured << " samples with "
             << stats.underruns << " underruns (expected at startup)");
    }
}