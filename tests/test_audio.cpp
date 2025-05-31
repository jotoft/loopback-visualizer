#include <catch2/catch_test_macros.hpp>
#include <audio_loopback/loopback_recorder.h>
#include <core/result.h>
#include <core/option.h>
#include <core/unit.h>

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
}