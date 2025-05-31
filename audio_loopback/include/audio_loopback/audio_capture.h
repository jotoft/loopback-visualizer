#pragma once
#include <audio_loopback/audio_ring_buffer.h>
#include <audio_loopback/loopback_recorder.h>
#include <atomic>
#include <memory>
#include <functional>

namespace audio {

// Interface for lock-free audio capture with built-in buffering
class AudioCapture {
public:
    static constexpr size_t DEFAULT_BUFFER_SIZE = 1 << 20; // 1M samples
    
    using ErrorCallback = std::function<void(AudioError)>;
    
    struct Config {
        size_t buffer_size = DEFAULT_BUFFER_SIZE;
        bool convert_to_mono = true;
        ErrorCallback error_callback = nullptr;
    };
    
    virtual ~AudioCapture() = default;
    
    // Start capture
    virtual core::Result<core::Unit, AudioError> start() = 0;
    
    // Stop capture
    virtual void stop() = 0;
    
    // Check if currently capturing
    virtual bool is_capturing() const = 0;
    
    // Read samples (consumes from buffer)
    virtual size_t read_samples(float* buffer, size_t count) = 0;
    
    // Peek at samples without consuming (for visualization)
    virtual size_t peek_samples(float* buffer, size_t count, size_t offset = 0) const = 0;
    
    // Get available samples to read
    virtual size_t available_samples() const = 0;
    
    // Buffer statistics
    struct Stats {
        size_t buffer_capacity;
        size_t available_samples;
        uint64_t total_samples_captured;
        uint64_t overruns;  // Samples dropped due to full buffer
        uint64_t underruns; // Read requests that couldn't be fully satisfied
    };
    
    virtual Stats get_stats() const = 0;
};

// Platform-specific implementations will inherit from this base
class AudioCaptureBase : public AudioCapture {
protected:
    using SampleBuffer = AudioRingBuffer<float, DEFAULT_BUFFER_SIZE>;
    
    Config config_;
    std::atomic<bool> capturing_{false};
    mutable SampleBuffer sample_buffer_;
    mutable std::atomic<uint64_t> total_samples_{0};
    mutable std::atomic<uint64_t> overruns_{0};
    mutable std::atomic<uint64_t> underruns_{0};
    
    AudioCaptureBase(Config config) : config_(std::move(config)) {}
    
public:
    bool is_capturing() const override {
        return capturing_.load(std::memory_order::relaxed);
    }
    
    size_t read_samples(float* buffer, size_t count) override {
        size_t read = sample_buffer_.read_bulk(buffer, count);
        if (read < count) {
            underruns_.fetch_add(1, std::memory_order::relaxed);
        }
        return read;
    }
    
    size_t peek_samples(float* buffer, size_t count, size_t offset) const override {
        return sample_buffer_.peek_bulk(buffer, count, offset);
    }
    
    size_t available_samples() const override {
        return sample_buffer_.available_read();
    }
    
    Stats get_stats() const override {
        return {
            config_.buffer_size,
            sample_buffer_.available_read(),
            total_samples_.load(std::memory_order::relaxed),
            overruns_.load(std::memory_order::relaxed),
            underruns_.load(std::memory_order::relaxed)
        };
    }
    
protected:
    // Called by platform-specific code from audio thread
    bool process_audio_callback(const AudioBuffer& buffer) {
        if (config_.convert_to_mono) {
            // Convert stereo to mono and write to ring buffer
            for (const auto& packet : buffer) {
                float mono = (packet.left + packet.right) * 0.5f;
                
                if (!sample_buffer_.try_write(mono)) {
                    overruns_.fetch_add(1, std::memory_order::relaxed);
                } else {
                    total_samples_.fetch_add(1, std::memory_order::relaxed);
                }
            }
        } else {
            // Interleaved stereo - write L,R,L,R...
            for (const auto& packet : buffer) {
                if (!sample_buffer_.try_write(packet.left) || 
                    !sample_buffer_.try_write(packet.right)) {
                    overruns_.fetch_add(1, std::memory_order::relaxed);
                    break; // Don't write partial stereo frames
                } else {
                    total_samples_.fetch_add(2, std::memory_order::relaxed);
                }
            }
        }
        
        return capturing_.load(std::memory_order::relaxed);
    }
};

} // namespace audio