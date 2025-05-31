#pragma once

#include <vector>
#include <deque>
#include <complex>
#include <cstddef>
#include "fft.h"

namespace visualization {

class FrequencyAnalyzer {
public:
    struct Config {
        size_t fft_size;           // FFT size (must be power of 2)
        float sample_rate;         // Audio sample rate
        size_t history_size;       // Number of FFT frames to keep in history
        float peak_threshold;      // Minimum magnitude for peak detection
        size_t max_peaks;          // Maximum number of peaks to track
        
        Config()
            : fft_size(2048)
            , sample_rate(44100.0f)
            , history_size(50)
            , peak_threshold(0.1f)
            , max_peaks(5) {}
    };
    
    struct FrequencyState {
        std::vector<float> magnitude_spectrum;  // Current magnitude spectrum
        std::vector<FrequencyPeak> peaks;       // Current detected peaks
        float dominant_frequency;               // Most dominant frequency
        float total_energy;                     // Total signal energy
    };
    
    FrequencyAnalyzer(const Config& config = Config());
    ~FrequencyAnalyzer() = default;
    
    // Process audio samples and update frequency analysis
    void process_samples(const float* samples, size_t count);
    
    // Get current frequency analysis state
    const FrequencyState& get_state() const { return current_state_; }
    
    // Get magnitude spectrum history for waterfall display
    const std::deque<std::vector<float>>& get_spectrum_history() const { return spectrum_history_; }
    
    // Get tracked frequency peaks over time
    const std::deque<std::vector<FrequencyPeak>>& get_peak_history() const { return peak_history_; }
    
    // Configuration
    void set_config(const Config& config);
    const Config& get_config() const { return config_; }
    
    // Apply window function to reduce spectral leakage
    static void apply_hann_window(std::vector<std::complex<float>>& data);
    
private:
    Config config_;
    FrequencyState current_state_;
    
    // Audio buffer for FFT
    std::vector<float> audio_buffer_;
    size_t buffer_write_pos_ = 0;
    
    // FFT working buffer
    std::vector<std::complex<float>> fft_buffer_;
    
    // History tracking
    std::deque<std::vector<float>> spectrum_history_;
    std::deque<std::vector<FrequencyPeak>> peak_history_;
    
    // Perform FFT analysis on current buffer
    void analyze_buffer();
};

} // namespace visualization