#pragma once

#include <vector>
#include <complex>
#include <cstddef>
#include "fft.h"

namespace visualization {

class FrequencyFilter {
public:
    struct Config {
        size_t fft_size;
        float sample_rate;
        float low_frequency;   // Low cutoff frequency in Hz
        float high_frequency;  // High cutoff frequency in Hz
        bool use_smooth_window; // Use smooth transition vs hard cutoff
        
        Config()
            : fft_size(2048)
            , sample_rate(44100.0f)
            , low_frequency(100.0f)
            , high_frequency(1000.0f)
            , use_smooth_window(true) {}
    };
    
    FrequencyFilter(const Config& config = Config());
    ~FrequencyFilter() = default;
    
    // Apply frequency band filter to input samples
    // Returns filtered samples (same size as input)
    std::vector<float> filter_samples(const float* samples, size_t count);
    
    // Get/set filter configuration
    void set_config(const Config& config);
    const Config& get_config() const { return config_; }
    
    // Get the current filter window (for visualization)
    const std::vector<float>& get_filter_window() const { return filter_window_; }
    
    // Update frequency range
    void set_frequency_range(float low_freq, float high_freq);
    
private:
    Config config_;
    
    // FFT buffers
    std::vector<std::complex<float>> fft_buffer_;
    std::vector<float> filter_window_;
    
    // Generate filter window based on frequency range
    void generate_filter_window();
    
    // Apply smooth transition window (raised cosine)
    float smooth_transition(float freq, float cutoff, float width);
};

} // namespace visualization