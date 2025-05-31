#include <catch2/catch_test_macros.hpp>
#include <vector>
#include <complex>
#include <cmath>
#include "visualization/fft.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

using namespace visualization;

TEST_CASE("FFT implementation", "[fft]") {
    
    SECTION("Power of two validation") {
        REQUIRE(is_power_of_two(1) == true);
        REQUIRE(is_power_of_two(2) == true);
        REQUIRE(is_power_of_two(4) == true);
        REQUIRE(is_power_of_two(8) == true);
        REQUIRE(is_power_of_two(16) == true);
        REQUIRE(is_power_of_two(3) == false);
        REQUIRE(is_power_of_two(5) == false);
        REQUIRE(is_power_of_two(6) == false);
        REQUIRE(is_power_of_two(0) == false);
    }
    
    SECTION("DC signal FFT") {
        std::vector<std::complex<float>> signal = {
            {1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 0.0f}
        };
        
        fft(signal);
        
        // DC component should be at index 0 with value 4.0
        REQUIRE(signal[0].real() == Approx(4.0f));
        REQUIRE(signal[0].imag() == Approx(0.0f));
        
        // All other components should be zero
        for (size_t i = 1; i < signal.size(); ++i) {
            REQUIRE(signal[i].real() == Approx(0.0f).margin(1e-6));
            REQUIRE(signal[i].imag() == Approx(0.0f).margin(1e-6));
        }
    }
    
    SECTION("Simple sine wave FFT") {
        // Single frequency sine wave: sin(2*pi*k*n/N) where k=1, N=8
        std::vector<std::complex<float>> signal(8);
        for (int i = 0; i < 8; ++i) {
            float angle = 2.0f * M_PI * 1 * i / 8.0f;
            signal[i] = {std::sin(angle), 0.0f};
        }
        
        fft(signal);
        
        // For a sine wave, energy should be at positive and negative frequency bins
        // For k=1 in 8-point FFT: energy at bins 1 and 7 (7 = 8-1)
        REQUIRE(std::abs(signal[1]) > 1.0f);  // Positive frequency
        REQUIRE(std::abs(signal[7]) > 1.0f);  // Negative frequency (conjugate)
        
        // DC and other bins should be near zero
        REQUIRE(std::abs(signal[0]) < 0.1f);
        REQUIRE(std::abs(signal[2]) < 0.1f);
        REQUIRE(std::abs(signal[3]) < 0.1f);
        REQUIRE(std::abs(signal[4]) < 0.1f);
    }
    
    SECTION("Inverse FFT roundtrip") {
        std::vector<std::complex<float>> original = {
            {1.0f, 0.0f}, {2.0f, 1.0f}, {-1.0f, 0.5f}, {0.5f, -2.0f}
        };
        std::vector<std::complex<float>> signal = original;
        
        fft(signal);
        ifft(signal);
        
        // Should recover original signal
        for (size_t i = 0; i < original.size(); ++i) {
            REQUIRE(signal[i].real() == Approx(original[i].real()).margin(1e-5));
            REQUIRE(signal[i].imag() == Approx(original[i].imag()).margin(1e-5));
        }
    }
}