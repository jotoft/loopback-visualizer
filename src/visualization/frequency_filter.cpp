#include "frequency_filter.h"
#include <cmath>
#include <algorithm>
#include <cstring>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace visualization {

FrequencyFilter::FrequencyFilter(const Config& config) 
    : config_(config) {
    // Validate FFT size
    if (!is_power_of_two(config_.fft_size)) {
        size_t n = 1;
        while (n < config_.fft_size) n <<= 1;
        config_.fft_size = n;
    }
    
    fft_buffer_.resize(config_.fft_size);
    filter_window_.resize(config_.fft_size);
    generate_filter_window();
}

std::vector<float> FrequencyFilter::filter_samples(const float* samples, size_t count) {
    std::vector<float> filtered(count);
    
    // Process in chunks of FFT size with overlap
    size_t overlap = config_.fft_size / 2;
    size_t hop_size = config_.fft_size - overlap;
    
    // Temporary output accumulator
    std::vector<float> output_accumulator(count + config_.fft_size, 0.0f);
    std::vector<float> window_accumulator(count + config_.fft_size, 0.0f);
    
    // Hann window for overlap-add
    std::vector<float> hann_window(config_.fft_size);
    for (size_t i = 0; i < config_.fft_size; ++i) {
        hann_window[i] = 0.5f * (1.0f - std::cos(2.0f * M_PI * i / (config_.fft_size - 1)));
    }
    
    // Process each frame
    for (size_t pos = 0; pos + config_.fft_size <= count + overlap; pos += hop_size) {
        // Fill FFT buffer with windowed samples
        for (size_t i = 0; i < config_.fft_size; ++i) {
            if (pos + i < count) {
                fft_buffer_[i] = std::complex<float>(samples[pos + i] * hann_window[i], 0.0f);
            } else {
                fft_buffer_[i] = std::complex<float>(0.0f, 0.0f);
            }
        }
        
        // Forward FFT
        fft(fft_buffer_);
        
        // Apply frequency filter
        for (size_t i = 0; i < config_.fft_size; ++i) {
            fft_buffer_[i] *= filter_window_[i];
        }
        
        // Inverse FFT
        ifft(fft_buffer_);
        
        // Overlap-add the result
        for (size_t i = 0; i < config_.fft_size; ++i) {
            if (pos + i < output_accumulator.size()) {
                output_accumulator[pos + i] += fft_buffer_[i].real() * hann_window[i];
                window_accumulator[pos + i] += hann_window[i] * hann_window[i];
            }
        }
    }
    
    // Normalize by window overlap
    for (size_t i = 0; i < count; ++i) {
        if (window_accumulator[i] > 0.0f) {
            filtered[i] = output_accumulator[i] / window_accumulator[i];
        } else {
            filtered[i] = 0.0f;
        }
    }
    
    return filtered;
}

void FrequencyFilter::set_config(const Config& config) {
    config_ = config;
    
    // Validate FFT size
    if (!is_power_of_two(config_.fft_size)) {
        size_t n = 1;
        while (n < config_.fft_size) n <<= 1;
        config_.fft_size = n;
    }
    
    fft_buffer_.resize(config_.fft_size);
    filter_window_.resize(config_.fft_size);
    generate_filter_window();
}

void FrequencyFilter::set_frequency_range(float low_freq, float high_freq) {
    config_.low_frequency = low_freq;
    config_.high_frequency = high_freq;
    generate_filter_window();
}

void FrequencyFilter::generate_filter_window() {
    float freq_resolution = config_.sample_rate / config_.fft_size;
    
    for (size_t i = 0; i < config_.fft_size; ++i) {
        float freq;
        
        // Handle both positive and negative frequencies
        if (i <= config_.fft_size / 2) {
            freq = i * freq_resolution;
        } else {
            freq = (config_.fft_size - i) * freq_resolution;
        }
        
        float gain = 0.0f;
        
        if (config_.use_smooth_window) {
            // Smooth transitions at cutoff frequencies
            float transition_width = 50.0f; // Hz
            
            if (freq >= config_.low_frequency && freq <= config_.high_frequency) {
                gain = 1.0f;
                
                // Apply smooth transitions at edges
                if (freq < config_.low_frequency + transition_width) {
                    gain *= smooth_transition(freq, config_.low_frequency, transition_width);
                }
                if (freq > config_.high_frequency - transition_width) {
                    gain *= smooth_transition(config_.high_frequency, freq, transition_width);
                }
            }
        } else {
            // Hard cutoff
            if (freq >= config_.low_frequency && freq <= config_.high_frequency) {
                gain = 1.0f;
            }
        }
        
        filter_window_[i] = gain;
    }
}

float FrequencyFilter::smooth_transition(float freq, float cutoff, float width) {
    float x = (freq - cutoff) / width;
    x = std::max(-1.0f, std::min(1.0f, x));
    // Raised cosine transition
    return 0.5f * (1.0f + std::cos(M_PI * (1.0f - x)));
}

} // namespace visualization