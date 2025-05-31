#include "phase_lock_analyzer.h"
#include "frequency_filter.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <memory>

namespace visualization {

PhaseLockAnalyzer::PhaseLockAnalyzer(const Config& config) 
    : config_(config) {
    phase_buffer_ = new float[config_.phase_buffer_size]();
    reference_window_ = new float[config_.correlation_window_size]();
    reference_accumulator_ = new float[config_.correlation_window_size]();
    
    if (config_.use_frequency_filter) {
        filtered_buffer_size_ = config_.phase_buffer_size;
        filtered_buffer_ = new float[filtered_buffer_size_]();
    }
}

PhaseLockAnalyzer::~PhaseLockAnalyzer() {
    delete[] phase_buffer_;
    delete[] reference_window_;
    delete[] reference_accumulator_;
    delete[] filtered_buffer_;
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
    
    // Apply frequency filter if enabled
    if (config_.use_frequency_filter) {
        apply_frequency_filter();
    }
    
    // Initialize reference window if we don't have one
    if (!has_reference_) {
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
    
    // If we found a good match that's different from last time, aggregate it
    if (max_correlation > config_.correlation_threshold && 
        std::abs(max_correlation - last_best_correlation_) > 0.01f) {
        
        // Extract the matching segment
        const float* buffer_to_use = config_.use_frequency_filter ? filtered_buffer_ : phase_buffer_;
        size_t match_start = (search_start + best_offset) % config_.phase_buffer_size;
        
        if (reference_count_ == 0) {
            // First match - copy directly
            for (int i = 0; i < config_.correlation_window_size; ++i) {
                reference_window_[i] = buffer_to_use[(match_start + i) % config_.phase_buffer_size];
            }
            reference_count_ = 1;
            has_reference_ = true;
        } else {
            // Use exponential moving average to blend new match into reference
            for (int i = 0; i < config_.correlation_window_size; ++i) {
                float new_sample = buffer_to_use[(match_start + i) % config_.phase_buffer_size];
                reference_window_[i] = (1.0f - ema_alpha_) * reference_window_[i] + ema_alpha_ * new_sample;
            }
            reference_count_++;
        }
        
        frames_since_good_match_ = 0;
    } else {
        frames_since_good_match_++;
        
        // Reset reference if we haven't had a good match for a while (2 seconds at 240fps)
        if (frames_since_good_match_ > 480 && has_reference_) {
            has_reference_ = false;
            reference_count_ = 0;
            frames_since_good_match_ = 0;
            // Next good match will reinitialize the reference
        }
    }
    
    last_best_correlation_ = max_correlation;
    
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
    std::memset(reference_accumulator_, 0, config_.correlation_window_size * sizeof(float));
    phase_write_pos_ = 0;
    has_reference_ = false;
    reference_count_ = 0;
    phase_offset_ = 0;
    target_phase_offset_ = 0;
    frames_since_reference_ = 0;
    frames_since_good_match_ = 0;
    last_best_correlation_ = 0.0f;
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
        delete[] reference_accumulator_;
        reference_window_ = new float[config.correlation_window_size];
        reference_accumulator_ = new float[config.correlation_window_size]();
        has_reference_ = false;
        reference_count_ = 0;
    }
    
    // Handle filtered buffer allocation
    if (config.use_frequency_filter && !filtered_buffer_) {
        filtered_buffer_size_ = config.phase_buffer_size;
        filtered_buffer_ = new float[filtered_buffer_size_]();
    } else if (!config.use_frequency_filter && filtered_buffer_) {
        delete[] filtered_buffer_;
        filtered_buffer_ = nullptr;
        filtered_buffer_size_ = 0;
    }
    
    config_ = config;
}

float PhaseLockAnalyzer::compute_correlation(size_t offset, size_t search_start) {
    float correlation = 0.0f;
    float ref_energy = 0.0f;
    float sig_energy = 0.0f;
    
    // Use filtered buffer if frequency filtering is enabled
    const float* buffer_to_use = config_.use_frequency_filter ? filtered_buffer_ : phase_buffer_;
    
    for (int i = 0; i < config_.correlation_window_size; ++i) {
        size_t idx = (search_start + offset + i) % config_.phase_buffer_size;
        float sig_val = buffer_to_use[idx];
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
    
    // Use filtered buffer if frequency filtering is enabled
    const float* buffer_to_use = config_.use_frequency_filter ? filtered_buffer_ : phase_buffer_;
    
    for (int i = 0; i < config_.correlation_window_size; ++i) {
        reference_window_[i] = buffer_to_use[(ref_start + i) % config_.phase_buffer_size];
    }
    
    has_reference_ = true;
    frames_since_reference_ = 0;
}

void PhaseLockAnalyzer::apply_frequency_filter() {
    if (!filtered_buffer_) return;
    
    // First, copy the entire phase buffer to filtered buffer
    // This ensures we always have something to display
    std::memcpy(filtered_buffer_, phase_buffer_, config_.phase_buffer_size * sizeof(float));
    
    // Create frequency filter with current config
    FrequencyFilter::Config filter_config;
    filter_config.fft_size = 512;  // Smaller FFT for lower latency
    filter_config.sample_rate = 44100.0f;
    filter_config.low_frequency = config_.filter_low_frequency;
    filter_config.high_frequency = config_.filter_high_frequency;
    filter_config.use_smooth_window = true;
    
    static std::unique_ptr<FrequencyFilter> filter;
    if (!filter || filter->get_config().low_frequency != config_.filter_low_frequency ||
        filter->get_config().high_frequency != config_.filter_high_frequency) {
        filter = std::make_unique<FrequencyFilter>(filter_config);
    }
    
    // Only filter the region we'll use for correlation
    // This reduces latency and keeps things synchronized
    const size_t filter_region_size = config_.display_samples + 2048;  // Extra for correlation search
    size_t start_pos = (phase_write_pos_ + config_.phase_buffer_size - filter_region_size) 
                      % config_.phase_buffer_size;
    
    // Extract the region to filter
    std::vector<float> samples(filter_region_size);
    for (size_t i = 0; i < filter_region_size; ++i) {
        samples[i] = phase_buffer_[(start_pos + i) % config_.phase_buffer_size];
    }
    
    // Apply frequency filter
    auto filtered = filter->filter_samples(samples.data(), samples.size());
    
    // Copy back only the filtered region
    for (size_t i = 0; i < filter_region_size; ++i) {
        filtered_buffer_[(start_pos + i) % config_.phase_buffer_size] = filtered[i];
    }
}

} // namespace visualization