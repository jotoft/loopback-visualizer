#include "simple_filters.h"
#include <cstring>

namespace visualization {

SimpleFilters::SimpleFilters(const Config& config) : config_(config) {
    update_highpass_coefficients();
    update_lowpass_coefficients();
    update_deesser_coefficients();
}

void SimpleFilters::process(float* samples, size_t count) {
    if (!samples || count == 0) return;
    
    // Process each sample through the filter chain
    for (size_t i = 0; i < count; ++i) {
        float sample = samples[i];
        
        // High-pass filter
        if (config_.highpass_enabled) {
            sample = highpass_.process(sample);
        }
        
        // Low-pass filter
        if (config_.lowpass_enabled) {
            sample = lowpass_.process(sample);
        }
        
        // De-esser
        if (config_.deesser_enabled) {
            // Detect sibilance in the specified frequency band
            float detected = deesser_detector_.process(sample);
            float envelope = std::abs(detected);
            
            // Simple envelope follower with fast attack, slow release
            const float attack = 0.001f;
            const float release = 0.01f;
            if (envelope > deesser_envelope_) {
                deesser_envelope_ = envelope * attack + deesser_envelope_ * (1.0f - attack);
            } else {
                deesser_envelope_ = envelope * release + deesser_envelope_ * (1.0f - release);
            }
            
            // Apply reduction if above threshold
            if (deesser_envelope_ > config_.deesser_threshold) {
                float reduction = 1.0f - (config_.deesser_ratio * 
                                         (deesser_envelope_ - config_.deesser_threshold) / 
                                         (1.0f - config_.deesser_threshold));
                sample *= reduction;
            }
        }
        
        // Soft clipping to prevent overflow
        if (sample > 1.0f) {
            sample = 1.0f - std::exp(-sample);
        } else if (sample < -1.0f) {
            sample = -1.0f + std::exp(sample);
        }
        
        samples[i] = sample;
    }
}

std::vector<float> SimpleFilters::process_copy(const float* samples, size_t count) {
    std::vector<float> output(count);
    if (samples && count > 0) {
        std::memcpy(output.data(), samples, count * sizeof(float));
        process(output.data(), count);
    }
    return output;
}

void SimpleFilters::reset() {
    highpass_.reset();
    lowpass_.reset();
    deesser_detector_.reset();
    deesser_envelope_ = 0.0f;
}

void SimpleFilters::set_config(const Config& config) {
    bool highpass_changed = (config.highpass_cutoff != config_.highpass_cutoff ||
                            config.highpass_resonance != config_.highpass_resonance ||
                            config.sample_rate != config_.sample_rate);
    
    bool lowpass_changed = (config.lowpass_cutoff != config_.lowpass_cutoff ||
                           config.lowpass_resonance != config_.lowpass_resonance ||
                           config.sample_rate != config_.sample_rate);
    
    bool deesser_changed = (config.deesser_frequency != config_.deesser_frequency ||
                           config.deesser_bandwidth != config_.deesser_bandwidth ||
                           config.sample_rate != config_.sample_rate);
    
    config_ = config;
    
    if (highpass_changed) {
        update_highpass_coefficients();
    }
    if (lowpass_changed) {
        update_lowpass_coefficients();
    }
    if (deesser_changed) {
        update_deesser_coefficients();
    }
}

void SimpleFilters::update_highpass_coefficients() {
    calculate_highpass_coefficients(config_.highpass_cutoff, config_.highpass_resonance, highpass_);
}

void SimpleFilters::update_lowpass_coefficients() {
    calculate_lowpass_coefficients(config_.lowpass_cutoff, config_.lowpass_resonance, lowpass_);
}

void SimpleFilters::update_deesser_coefficients() {
    calculate_bandpass_coefficients(config_.deesser_frequency, config_.deesser_bandwidth, deesser_detector_);
}

void SimpleFilters::calculate_highpass_coefficients(float cutoff, float q, BiquadFilter& filter) {
    // Butterworth high-pass filter
    float omega = 2.0f * M_PI * cutoff / config_.sample_rate;
    float sin_omega = std::sin(omega);
    float cos_omega = std::cos(omega);
    float alpha = sin_omega / (2.0f * q);
    
    filter.b0 = (1.0f + cos_omega) / 2.0f;
    filter.b1 = -(1.0f + cos_omega);
    filter.b2 = (1.0f + cos_omega) / 2.0f;
    filter.a0 = 1.0f + alpha;
    filter.a1 = -2.0f * cos_omega;
    filter.a2 = 1.0f - alpha;
}

void SimpleFilters::calculate_lowpass_coefficients(float cutoff, float q, BiquadFilter& filter) {
    // Butterworth low-pass filter
    float omega = 2.0f * M_PI * cutoff / config_.sample_rate;
    float sin_omega = std::sin(omega);
    float cos_omega = std::cos(omega);
    float alpha = sin_omega / (2.0f * q);
    
    filter.b0 = (1.0f - cos_omega) / 2.0f;
    filter.b1 = 1.0f - cos_omega;
    filter.b2 = (1.0f - cos_omega) / 2.0f;
    filter.a0 = 1.0f + alpha;
    filter.a1 = -2.0f * cos_omega;
    filter.a2 = 1.0f - alpha;
}

void SimpleFilters::calculate_bandpass_coefficients(float center, float bandwidth, BiquadFilter& filter) {
    // Bandpass filter for de-esser detection
    float omega = 2.0f * M_PI * center / config_.sample_rate;
    float sin_omega = std::sin(omega);
    float cos_omega = std::cos(omega);
    float q = center / bandwidth;
    float alpha = sin_omega / (2.0f * q);
    
    filter.b0 = alpha;
    filter.b1 = 0.0f;
    filter.b2 = -alpha;
    filter.a0 = 1.0f + alpha;
    filter.a1 = -2.0f * cos_omega;
    filter.a2 = 1.0f - alpha;
}

} // namespace visualization