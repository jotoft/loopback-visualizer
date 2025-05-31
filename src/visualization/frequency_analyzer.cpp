#include "frequency_analyzer.h"
#include <cmath>
#include <numeric>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace visualization {

FrequencyAnalyzer::FrequencyAnalyzer(const Config& config) 
    : config_(config) {
    // Validate FFT size is power of 2
    if (!is_power_of_two(config_.fft_size)) {
        // Round up to next power of 2
        size_t n = 1;
        while (n < config_.fft_size) n <<= 1;
        config_.fft_size = n;
    }
    
    // Initialize buffers
    audio_buffer_.resize(config_.fft_size, 0.0f);
    fft_buffer_.resize(config_.fft_size);
    current_state_.magnitude_spectrum.resize(config_.fft_size);
}

void FrequencyAnalyzer::process_samples(const float* samples, size_t count) {
    // Add samples to circular buffer
    for (size_t i = 0; i < count; ++i) {
        audio_buffer_[buffer_write_pos_] = samples[i];
        buffer_write_pos_ = (buffer_write_pos_ + 1) % config_.fft_size;
    }
    
    // Perform analysis if we have enough new samples
    static size_t samples_since_analysis = 0;
    samples_since_analysis += count;
    
    // Analyze every fft_size/4 samples for overlap
    if (samples_since_analysis >= config_.fft_size / 4) {
        analyze_buffer();
        samples_since_analysis = 0;
    }
}

void FrequencyAnalyzer::analyze_buffer() {
    // Copy audio data to FFT buffer, starting from oldest sample
    size_t read_pos = buffer_write_pos_;
    for (size_t i = 0; i < config_.fft_size; ++i) {
        fft_buffer_[i] = std::complex<float>(audio_buffer_[read_pos], 0.0f);
        read_pos = (read_pos + 1) % config_.fft_size;
    }
    
    // Apply window function to reduce spectral leakage
    apply_hann_window(fft_buffer_);
    
    // Perform FFT
    fft(fft_buffer_);
    
    // Compute magnitude spectrum
    current_state_.magnitude_spectrum = compute_magnitude_spectrum(fft_buffer_);
    
    // Normalize by FFT size
    for (auto& mag : current_state_.magnitude_spectrum) {
        mag /= config_.fft_size;
    }
    
    // Find peaks
    current_state_.peaks = find_peaks(current_state_.magnitude_spectrum, 
                                     config_.sample_rate,
                                     config_.peak_threshold,
                                     config_.max_peaks);
    
    // Set dominant frequency
    if (!current_state_.peaks.empty()) {
        current_state_.dominant_frequency = current_state_.peaks[0].frequency;
    } else {
        current_state_.dominant_frequency = 0.0f;
    }
    
    // Calculate total energy (sum of squared magnitudes)
    current_state_.total_energy = 0.0f;
    for (size_t i = 0; i < config_.fft_size / 2; ++i) {
        float mag = current_state_.magnitude_spectrum[i];
        current_state_.total_energy += mag * mag;
    }
    
    // Update history
    spectrum_history_.push_back(current_state_.magnitude_spectrum);
    if (spectrum_history_.size() > config_.history_size) {
        spectrum_history_.pop_front();
    }
    
    peak_history_.push_back(current_state_.peaks);
    if (peak_history_.size() > config_.history_size) {
        peak_history_.pop_front();
    }
}

void FrequencyAnalyzer::set_config(const Config& config) {
    if (config.fft_size != config_.fft_size) {
        // Resize buffers if FFT size changed
        config_ = config;
        
        // Validate FFT size
        if (!is_power_of_two(config_.fft_size)) {
            size_t n = 1;
            while (n < config_.fft_size) n <<= 1;
            config_.fft_size = n;
        }
        
        audio_buffer_.clear();
        audio_buffer_.resize(config_.fft_size, 0.0f);
        fft_buffer_.resize(config_.fft_size);
        current_state_.magnitude_spectrum.resize(config_.fft_size);
        buffer_write_pos_ = 0;
        
        // Clear history
        spectrum_history_.clear();
        peak_history_.clear();
    } else {
        config_ = config;
    }
}

void FrequencyAnalyzer::apply_hann_window(std::vector<std::complex<float>>& data) {
    size_t n = data.size();
    for (size_t i = 0; i < n; ++i) {
        float window = 0.5f * (1.0f - std::cos(2.0f * M_PI * i / (n - 1)));
        data[i] *= window;
    }
}

} // namespace visualization