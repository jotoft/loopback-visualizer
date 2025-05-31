#include "fft.h"
#include <algorithm>
#include <numeric>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace visualization {

// Forward FFT using Cooley-Tukey algorithm
void fft(std::vector<std::complex<float>>& data) {
    size_t n = data.size();
    
    // Base case
    if (n <= 1) return;
    
    // Bit-reversal permutation
    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1) {
            j ^= bit;
        }
        j ^= bit;
        
        if (i < j) {
            std::swap(data[i], data[j]);
        }
    }
    
    // Cooley-Tukey FFT
    for (size_t len = 2; len <= n; len <<= 1) {
        float angle = -2.0f * M_PI / len;
        std::complex<float> wlen(std::cos(angle), std::sin(angle));
        
        for (size_t i = 0; i < n; i += len) {
            std::complex<float> w(1.0f, 0.0f);
            for (size_t j = 0; j < len / 2; ++j) {
                std::complex<float> u = data[i + j];
                std::complex<float> v = data[i + j + len / 2] * w;
                data[i + j] = u + v;
                data[i + j + len / 2] = u - v;
                w *= wlen;
            }
        }
    }
}

// Inverse FFT
void ifft(std::vector<std::complex<float>>& data) {
    size_t n = data.size();
    
    // Conjugate the complex numbers
    for (auto& x : data) {
        x = std::conj(x);
    }
    
    // Forward FFT
    fft(data);
    
    // Conjugate and scale
    for (auto& x : data) {
        x = std::conj(x) / static_cast<float>(n);
    }
}

// Compute magnitude spectrum
std::vector<float> compute_magnitude_spectrum(const std::vector<std::complex<float>>& fft_data) {
    std::vector<float> magnitude(fft_data.size());
    for (size_t i = 0; i < fft_data.size(); ++i) {
        magnitude[i] = std::abs(fft_data[i]);
    }
    return magnitude;
}

// Compute power spectrum
std::vector<float> compute_power_spectrum(const std::vector<std::complex<float>>& fft_data) {
    std::vector<float> power(fft_data.size());
    for (size_t i = 0; i < fft_data.size(); ++i) {
        float mag = std::abs(fft_data[i]);
        power[i] = mag * mag;
    }
    return power;
}

// Find peaks in magnitude spectrum
std::vector<FrequencyPeak> find_peaks(const std::vector<float>& magnitude_spectrum,
                                     float sample_rate,
                                     float threshold,
                                     size_t max_peaks) {
    std::vector<FrequencyPeak> peaks;
    
    // Only search up to Nyquist frequency (half the spectrum)
    size_t n = magnitude_spectrum.size();
    size_t half_n = n / 2;
    
    // Find local maxima above threshold
    for (size_t i = 1; i < half_n - 1; ++i) {
        float mag = magnitude_spectrum[i];
        
        // Check if it's a local maximum and above threshold
        if (mag > threshold && 
            mag > magnitude_spectrum[i - 1] && 
            mag > magnitude_spectrum[i + 1]) {
            
            FrequencyPeak peak;
            peak.bin = i;
            peak.magnitude = mag;
            peak.frequency = (i * sample_rate) / n;
            peaks.push_back(peak);
        }
    }
    
    // Sort by magnitude (descending)
    std::sort(peaks.begin(), peaks.end(), 
              [](const FrequencyPeak& a, const FrequencyPeak& b) {
                  return a.magnitude > b.magnitude;
              });
    
    // Keep only top peaks
    if (peaks.size() > max_peaks) {
        peaks.resize(max_peaks);
    }
    
    return peaks;
}

} // namespace visualization