// Example of using the new lock-free audio capture API
#include <audio_loopback/loopback_recorder.h>
#include <audio_loopback/audio_capture.h>
#include <iostream>
#include <thread>
#include <chrono>

int main() {
    // Get default audio device
    auto sink_opt = audio::get_default_sink(false);
    if (sink_opt.is_none()) {
        std::cerr << "No audio device found\n";
        return 1;
    }
    
    auto sink = sink_opt.unwrap();
    std::cout << "Using: " << sink.name << "\n";
    
    // Create audio capture with lock-free buffer
    auto capture = audio::create_audio_capture(sink);
    
    // Start capture
    auto result = capture->start();
    if (result.is_err()) {
        std::cerr << "Failed to start capture\n";
        return 1;
    }
    
    // Main loop - read and process audio
    for (int i = 0; i < 100; ++i) { // Run for ~1.6 seconds
        // Read samples for processing
        constexpr size_t chunk_size = 1024;
        float samples[chunk_size];
        
        size_t read = capture->read_samples(samples, chunk_size);
        
        if (read > 0) {
            // Process samples...
            float sum = 0;
            for (size_t j = 0; j < read; ++j) {
                sum += std::abs(samples[j]);
            }
            float avg = sum / read;
            
            // Simple VU meter visualization
            int bars = static_cast<int>(avg * 50);
            std::cout << "\r[";
            for (int b = 0; b < 50; ++b) {
                std::cout << (b < bars ? '#' : ' ');
            }
            std::cout << "] " << avg << "  " << std::flush;
        }
        
        // Check stats periodically
        if (i % 50 == 0) {
            auto stats = capture->get_stats();
            std::cout << "\nStats - Available: " << stats.available_samples 
                      << ", Total: " << stats.total_samples_captured
                      << ", Overruns: " << stats.overruns
                      << ", Underruns: " << stats.underruns << "\n";
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(16)); // ~60 FPS
    }
    
    std::cout << "\nStopping capture...\n";
    capture->stop();
    
    return 0;
}