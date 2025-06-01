#pragma once

#include <vector>
#include <cmath>
#include <algorithm>

namespace visualization {

class SimpleFilters {
public:
    struct Config {
        // High-pass filter
        bool highpass_enabled;
        float highpass_cutoff;  // Hz
        float highpass_resonance;  // Q factor
        
        // Low-pass filter
        bool lowpass_enabled;
        float lowpass_cutoff;  // Hz
        float lowpass_resonance;
        
        // De-esser
        bool deesser_enabled;
        float deesser_frequency;  // Center frequency
        float deesser_threshold;     // Detection threshold
        float deesser_ratio;         // Reduction amount (0-1)
        float deesser_bandwidth;  // Bandwidth in Hz
        
        float sample_rate;
        
        Config() :
            highpass_enabled(true),   // Enabled by default
            highpass_cutoff(90.0f),   // 90 Hz default
            highpass_resonance(0.5f), // 0.5 resonance
            lowpass_enabled(false),
            lowpass_cutoff(8000.0f),
            lowpass_resonance(0.707f),
            deesser_enabled(false),
            deesser_frequency(5000.0f),
            deesser_threshold(0.5f),
            deesser_ratio(0.5f),
            deesser_bandwidth(2000.0f),
            sample_rate(44100.0f)
        {}
    };
    
    SimpleFilters(const Config& config = Config());
    ~SimpleFilters() = default;
    
    // Process a buffer of samples in-place
    void process(float* samples, size_t count);
    
    // Process and return new buffer
    std::vector<float> process_copy(const float* samples, size_t count);
    
    // Reset filter states
    void reset();
    
    // Configuration
    void set_config(const Config& config);
    const Config& get_config() const { return config_; }
    
    // Get current de-esser envelope level (for UI display)
    float get_deesser_envelope() const { return deesser_envelope_; }
    
private:
    Config config_;
    
    // Biquad filter coefficients and states
    struct BiquadFilter {
        float a0 = 1.0f, a1 = 0.0f, a2 = 0.0f;
        float b0 = 1.0f, b1 = 0.0f, b2 = 0.0f;
        float x1 = 0.0f, x2 = 0.0f;  // Input history
        float y1 = 0.0f, y2 = 0.0f;  // Output history
        
        void reset() {
            x1 = x2 = y1 = y2 = 0.0f;
        }
        
        float process(float input) {
            float output = (b0 * input + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2) / a0;
            x2 = x1;
            x1 = input;
            y2 = y1;
            y1 = output;
            return output;
        }
    };
    
    BiquadFilter highpass_;
    BiquadFilter lowpass_;
    
    // De-esser components
    BiquadFilter deesser_detector_;  // Bandpass for detection
    float deesser_envelope_ = 0.0f;
    
    void update_highpass_coefficients();
    void update_lowpass_coefficients();
    void update_deesser_coefficients();
    
    // Helper to calculate biquad coefficients
    void calculate_highpass_coefficients(float cutoff, float q, BiquadFilter& filter);
    void calculate_lowpass_coefficients(float cutoff, float q, BiquadFilter& filter);
    void calculate_bandpass_coefficients(float center, float bandwidth, BiquadFilter& filter);
};

} // namespace visualization