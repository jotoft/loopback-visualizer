#pragma once

#include <vector>
#include <deque>
#include <cstddef>
#include <cmath>

namespace visualization {

class PhaseLockAnalyzer {
public:
    enum class ReferenceMode {
        ACCUMULATOR,  // True average with periodic reset
        EMA          // Exponential moving average
    };
    
    struct Config {
        float phase_smoothing;
        float correlation_threshold;
        int correlation_window_size;
        size_t phase_buffer_size;
        size_t display_samples;
        bool use_frequency_filter;
        float filter_low_frequency;
        float filter_high_frequency;
        ReferenceMode reference_mode;
        int accumulator_reset_count;
        
        Config() 
            : phase_smoothing(0.0f)
            , correlation_threshold(0.45f)
            , correlation_window_size(300)
            , phase_buffer_size(4096)
            , display_samples(2400)
            , use_frequency_filter(false)
            , filter_low_frequency(100.0f)
            , filter_high_frequency(1000.0f)
            , reference_mode(ReferenceMode::ACCUMULATOR)
            , accumulator_reset_count(50) {}
    };
    
    struct State {
        float best_correlation = 0.0f;
        bool has_lock = false;
        size_t read_position = 0;
    };
    
    PhaseLockAnalyzer(const Config& config = Config());
    ~PhaseLockAnalyzer();
    
    // Add audio samples to the analyzer
    void add_samples(const float* samples, size_t count);
    
    // Get the current read position for phase-locked display
    State analyze(bool phase_lock_enabled);
    
    // Reset the analyzer state
    void reset();
    
    // Update configuration
    void set_config(const Config& config);
    const Config& get_config() const { return config_; }
    
    // Get the internal phase buffer for display
    const float* get_phase_buffer() const { return phase_buffer_; }
    size_t get_phase_buffer_size() const { return config_.phase_buffer_size; }
    
    // Get correlation history for visualization
    const std::deque<float>& get_correlation_history() const { return correlation_history_; }
    
    // Get reference window for visualization
    const float* get_reference_window() const { return reference_window_; }
    bool has_reference() const { return has_reference_; }
    int get_reference_count() const { return reference_count_; }
    
    // Get filtered buffer for visualization
    const float* get_filtered_buffer() const { return filtered_buffer_; }
    size_t get_filtered_buffer_size() const { return filtered_buffer_size_; }
    
    // Get/set EMA alpha for reference averaging
    float get_ema_alpha() const { return ema_alpha_; }
    void set_ema_alpha(float alpha) { ema_alpha_ = std::max(0.01f, std::min(0.5f, alpha)); }

private:
    Config config_;
    
    // Phase tracking buffers
    float* phase_buffer_;
    size_t phase_write_pos_ = 0;
    
    // Filtered buffer for frequency-selective correlation
    float* filtered_buffer_ = nullptr;
    size_t filtered_buffer_size_ = 0;
    
    // Cross-correlation state
    float* reference_window_ = nullptr;
    float* reference_accumulator_ = nullptr;  // For averaging multiple good matches
    int reference_count_ = 0;                 // Number of samples accumulated
    bool has_reference_ = false;
    size_t phase_offset_ = 0;
    size_t target_phase_offset_ = 0;
    int frames_since_reference_ = 0;
    int frames_since_good_match_ = 0;        // Frames since last good correlation
    float last_best_correlation_ = 0.0f;     // Track previous best correlation
    float ema_alpha_ = 0.1f;                 // EMA blending factor
    
    // Correlation history
    std::deque<float> correlation_history_;
    static constexpr size_t max_history_ = 240;  // 1 second at 240 FPS
    
    // Helper methods
    float compute_correlation(size_t offset, size_t search_start);
    void update_reference_window();
    void apply_frequency_filter();
};

} // namespace visualization