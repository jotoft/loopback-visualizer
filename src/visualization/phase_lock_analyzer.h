#pragma once

#include <vector>
#include <deque>
#include <cstddef>
#include <cmath>

namespace visualization {

class PhaseLockAnalyzer {
public:
    struct Config {
        float phase_smoothing;
        float correlation_threshold;
        int correlation_window_size;
        size_t phase_buffer_size;
        size_t display_samples;
        
        Config() 
            : phase_smoothing(0.0f)
            , correlation_threshold(0.45f)
            , correlation_window_size(300)
            , phase_buffer_size(4096)
            , display_samples(2400) {}
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

private:
    Config config_;
    
    // Phase tracking buffers
    float* phase_buffer_;
    size_t phase_write_pos_ = 0;
    
    // Cross-correlation state
    float* reference_window_ = nullptr;
    bool has_reference_ = false;
    size_t phase_offset_ = 0;
    size_t target_phase_offset_ = 0;
    int frames_since_reference_ = 0;
    
    // Correlation history
    std::deque<float> correlation_history_;
    static constexpr size_t max_history_ = 240;  // 1 second at 240 FPS
    
    // Helper methods
    float compute_correlation(size_t offset, size_t search_start);
    void update_reference_window();
};

} // namespace visualization