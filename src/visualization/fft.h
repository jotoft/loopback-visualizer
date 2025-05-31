#pragma once

#include <vector>
#include <complex>
#include <cmath>

namespace visualization {

// Check if a number is a power of two
inline bool is_power_of_two(size_t n) {
    return n > 0 && (n & (n - 1)) == 0;
}

// Cooley-Tukey FFT implementation
void fft(std::vector<std::complex<float>>& data);

// Inverse FFT
void ifft(std::vector<std::complex<float>>& data);

// Compute magnitude spectrum from complex FFT output
std::vector<float> compute_magnitude_spectrum(const std::vector<std::complex<float>>& fft_data);

// Compute power spectrum (magnitude squared) from complex FFT output  
std::vector<float> compute_power_spectrum(const std::vector<std::complex<float>>& fft_data);

// Find peak frequencies in magnitude spectrum
struct FrequencyPeak {
    float frequency;   // Frequency in Hz
    float magnitude;   // Magnitude at this frequency
    size_t bin;       // FFT bin index
};

std::vector<FrequencyPeak> find_peaks(const std::vector<float>& magnitude_spectrum, 
                                     float sample_rate,
                                     float threshold = 0.1f,
                                     size_t max_peaks = 10);

} // namespace visualization