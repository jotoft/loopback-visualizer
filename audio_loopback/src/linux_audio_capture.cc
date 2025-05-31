#include <audio_loopback/audio_capture.h>
#include <pulse/simple.h>
#include <pulse/error.h>
#include <thread>
#include <iostream>
#include <memory>

namespace audio {

namespace {
    static const pa_sample_spec SAMPLE_SPEC = {
        .format = PA_SAMPLE_FLOAT32,
        .rate = 44100,
        .channels = 2
    };
}

class LinuxAudioCapture : public AudioCaptureBase {
private:
    std::thread capture_thread_;
    pa_simple* pulse_handle_ = nullptr;
    
public:
    LinuxAudioCapture(const AudioSinkInfo& sink, Config config) 
        : AudioCaptureBase(std::move(config)) {
        // Note: We don't initialize PulseAudio here to avoid blocking the constructor
    }
    
    ~LinuxAudioCapture() override {
        stop();
    }
    
    core::Result<core::Unit, AudioError> start() override {
        if (capturing_.load()) {
            return core::Result<core::Unit, AudioError>::Err(AudioError::SystemError);
        }
        
        capturing_.store(true);
        
        // Start capture thread
        capture_thread_ = std::thread([this]() {
            capture_loop();
        });
        
        return core::Result<core::Unit, AudioError>::Ok(core::unit);
    }
    
    void stop() override {
        capturing_.store(false);
        
        if (capture_thread_.joinable()) {
            capture_thread_.join();
        }
    }
    
private:
    void capture_loop() {
        // Initialize PulseAudio in the audio thread
        const char* monitor_device = "@DEFAULT_MONITOR@";
        int error = 0;
        
        // Configure ultra low-latency buffer attributes
        pa_buffer_attr buffer_attr = {};
        buffer_attr.fragsize = 64 * sizeof(float) * 2;  // 64 samples = ~1.5ms at 44.1kHz
        buffer_attr.maxlength = 128 * sizeof(float) * 2;  // Small max buffer
        buffer_attr.tlength = (uint32_t) -1;
        buffer_attr.prebuf = (uint32_t) -1;
        buffer_attr.minreq = (uint32_t) -1;
        
        pulse_handle_ = pa_simple_new(
            NULL,               // Use default server
            "Visualizer",       // Application name
            PA_STREAM_RECORD,
            monitor_device,     // Use monitor device for loopback
            "Audio Loopback",   // Stream description
            &SAMPLE_SPEC,
            NULL,               // Use default channel map
            &buffer_attr,       // Low-latency buffer attributes
            &error
        );
        
        if (!pulse_handle_) {
            if (config_.error_callback) {
                config_.error_callback(AudioError::InitializationFailed);
            }
            return;
        }
        
        // Main capture loop with minimal buffer
        static constexpr size_t BUFFER_SIZE = 64;  // Ultra-low latency
        AudioBuffer buffer;
        buffer.reserve(BUFFER_SIZE);
        
        while (capturing_.load(std::memory_order::relaxed)) {
            buffer.clear();
            StereoPacket raw_buffer[BUFFER_SIZE];
            
            int read_error = 0;
            if (pa_simple_read(pulse_handle_, raw_buffer, sizeof(raw_buffer), &read_error) < 0) {
                if (config_.error_callback) {
                    config_.error_callback(AudioError::ReadError);
                }
                break;
            }
            
            // Copy to AudioBuffer
            std::copy(std::begin(raw_buffer), std::end(raw_buffer), std::back_inserter(buffer));
            
            // Process through base class (handles ring buffer)
            process_audio_callback(buffer);
        }
        
        // Cleanup
        if (pulse_handle_) {
            pa_simple_free(pulse_handle_);
            pulse_handle_ = nullptr;
        }
    }
};

// Factory function implementation
std::unique_ptr<AudioCapture> create_audio_capture(const AudioSinkInfo& sink) {
    AudioCapture::Config config;
    config.buffer_size = 4096;  // Small ring buffer for low latency
    config.convert_to_mono = true;
    
    return std::make_unique<LinuxAudioCapture>(sink, std::move(config));
}

} // namespace audio