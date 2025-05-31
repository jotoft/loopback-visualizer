#include "phase_lock_analyzer.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace visualization {

PhaseLockAnalyzer::PhaseLockAnalyzer(const Config& config) 
    : config_(config) {
    phase_buffer_ = new float[config_.phase_buffer_size]();
    reference_window_ = new float[config_.correlation_window_size];
}

PhaseLockAnalyzer::~PhaseLockAnalyzer() {
    delete[] phase_buffer_;
    delete[] reference_window_;
}

void PhaseLockAnalyzer::add_samples(const float* samples, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        phase_buffer_[phase_write_pos_] = samples[i];
        phase_write_pos_ = (phase_write_pos_ + 1) % config_.phase_buffer_size;
    }
}

PhaseLockAnalyzer::State PhaseLockAnalyzer::analyze(bool phase_lock_enabled) {
    State state;
    
    if (!phase_lock_enabled) {
        // No phase lock - absolute minimum latency
        state.read_position = (phase_write_pos_ + config_.phase_buffer_size - config_.display_samples) 
                            % config_.phase_buffer_size;
        state.has_lock = false;
        state.best_correlation = 0.0f;
        has_reference_ = false;
        phase_offset_ = 0;
        correlation_history_.clear();
        return state;
    }
    
    // Update reference window periodically
    if (!has_reference_ || frames_since_reference_ > 120) {  // ~0.5 seconds at 240fps
        update_reference_window();
    }
    frames_since_reference_++;
    
    // Find best correlation offset
    float max_correlation = -1.0f;
    size_t best_offset = 0;
    
    const size_t SEARCH_RANGE = 1024;
    size_t search_start = (phase_write_pos_ + config_.phase_buffer_size - 
                          config_.display_samples - SEARCH_RANGE) % config_.phase_buffer_size;
    
    // Coarse search
    for (size_t offset = 0; offset < SEARCH_RANGE; offset += 4) {
        float correlation = compute_correlation(offset, search_start);
        if (correlation > max_correlation) {
            max_correlation = correlation;
            best_offset = offset;
        }
    }
    
    // Fine-tune around best offset
    if (best_offset > 2 && best_offset < SEARCH_RANGE - 2) {
        for (int fine_offset = -2; fine_offset <= 2; ++fine_offset) {
            size_t offset = best_offset + fine_offset;
            float correlation = compute_correlation(offset, search_start);
            if (correlation > max_correlation) {
                max_correlation = correlation;
                best_offset = offset;
            }
        }
    }
    
    state.best_correlation = max_correlation;
    
    // Update correlation history
    correlation_history_.push_back(state.best_correlation);
    if (correlation_history_.size() > max_history_) {
        correlation_history_.pop_front();
    }
    
    // Update target phase offset based on correlation
    if (max_correlation > config_.correlation_threshold) {
        target_phase_offset_ = (search_start + best_offset) % config_.phase_buffer_size;
        state.has_lock = true;
    } else {
        // Fallback to recent data if correlation is poor
        target_phase_offset_ = (phase_write_pos_ + config_.phase_buffer_size - 
                               config_.display_samples) % config_.phase_buffer_size;
        state.has_lock = false;
    }
    
    // Smoothly interpolate to the target phase offset
    if (phase_offset_ == 0) {
        // First time - jump directly to target
        phase_offset_ = target_phase_offset_;
    } else {
        // Calculate phase difference (handling wraparound)
        int phase_diff = (int)target_phase_offset_ - (int)phase_offset_;
        
        // Handle wraparound
        if (phase_diff > (int)(config_.phase_buffer_size / 2)) {
            phase_diff -= config_.phase_buffer_size;
        } else if (phase_diff < -(int)(config_.phase_buffer_size / 2)) {
            phase_diff += config_.phase_buffer_size;
        }
        
        // Apply smoothing
        float smooth_diff = phase_diff * (1.0f - config_.phase_smoothing);
        phase_offset_ = (phase_offset_ + (int)smooth_diff + config_.phase_buffer_size) 
                       % config_.phase_buffer_size;
    }
    
    state.read_position = phase_offset_;
    return state;
}

void PhaseLockAnalyzer::reset() {
    std::memset(phase_buffer_, 0, config_.phase_buffer_size * sizeof(float));
    phase_write_pos_ = 0;
    has_reference_ = false;
    phase_offset_ = 0;
    target_phase_offset_ = 0;
    frames_since_reference_ = 0;
    correlation_history_.clear();
}

void PhaseLockAnalyzer::set_config(const Config& config) {
    if (config.phase_buffer_size != config_.phase_buffer_size) {
        delete[] phase_buffer_;
        phase_buffer_ = new float[config.phase_buffer_size]();
        reset();
    }
    
    if (config.correlation_window_size != config_.correlation_window_size) {
        delete[] reference_window_;
        reference_window_ = new float[config.correlation_window_size];
        has_reference_ = false;
    }
    
    config_ = config;
}

float PhaseLockAnalyzer::compute_correlation(size_t offset, size_t search_start) {
    float correlation = 0.0f;
    float ref_energy = 0.0f;
    float sig_energy = 0.0f;
    
    for (int i = 0; i < config_.correlation_window_size; ++i) {
        size_t idx = (search_start + offset + i) % config_.phase_buffer_size;
        float sig_val = phase_buffer_[idx];
        float ref_val = reference_window_[i];
        
        correlation += sig_val * ref_val;
        sig_energy += sig_val * sig_val;
        ref_energy += ref_val * ref_val;
    }
    
    // Normalize correlation
    if (sig_energy > 0.0f && ref_energy > 0.0f) {
        correlation /= sqrtf(sig_energy * ref_energy);
    } else {
        correlation = 0.0f;
    }
    
    return correlation;
}

void PhaseLockAnalyzer::update_reference_window() {
    size_t ref_start = (phase_write_pos_ + config_.phase_buffer_size - 
                       config_.correlation_window_size) % config_.phase_buffer_size;
    
    for (int i = 0; i < config_.correlation_window_size; ++i) {
        reference_window_[i] = phase_buffer_[(ref_start + i) % config_.phase_buffer_size];
    }
    
    has_reference_ = true;
    frames_since_reference_ = 0;
}

} // namespace visualization