# Cross-Correlation Phase Locking

## Overview

The loopback visualizer now features advanced cross-correlation based phase locking, providing stable waveform display similar to a traditional oscilloscope. This technique finds the best phase alignment between consecutive frames of audio, ensuring the waveform appears stationary rather than scrolling.

## How It Works

### 1. Reference Window Capture
- The system captures a 512-sample reference window from the audio stream
- This reference is updated every ~0.5 seconds (120 frames at 240 FPS)
- The reference represents the "pattern" we're trying to lock onto

### 2. Cross-Correlation Search
The algorithm performs normalized cross-correlation to find where the current audio best matches the reference:

```
correlation = Σ(signal[i] × reference[i]) / √(Σsignal²[i] × Σreference²[i])
```

Key features:
- **Coarse search**: Checks every 4th sample for speed
- **Fine tuning**: Refines the match within ±2 samples
- **Normalization**: Makes correlation independent of signal amplitude
- **Search range**: Looks back up to 1024 samples

### 3. Smooth Phase Transitions
To prevent jarring jumps, the system smoothly interpolates between phase offsets:
- Uses exponential smoothing with factor 0.9 (adjustable)
- Handles circular buffer wraparound correctly
- Provides smooth, natural-looking transitions

### 4. Visual Feedback
The waveform color indicates lock quality:
- **Green**: Excellent lock (correlation > 0.7)
- **Yellow**: Moderate lock (correlation 0.5-0.7)
- **Red**: Poor lock (correlation < 0.5)
- **Blue bar**: Shows correlation strength at bottom of screen

## Usage

### Controls
- **P**: Toggle phase lock ON/OFF (default: OFF for minimum latency)
- **I**: Switch between input (microphone) and loopback (system audio)
- **ESC/Q**: Quit

### Performance Characteristics
- **Latency (phase lock OFF)**: ~5ms ultra-low latency
- **Latency (phase lock ON)**: ~10-15ms (due to correlation window)
- **Frame rate**: Locked at 240 FPS for smooth visualization
- **CPU usage**: Minimal (<5% for correlation computation)

## Implementation Details

### Buffer Architecture
```
Audio Thread → Lock-free Ring Buffer → Render Thread
                                         ↓
                                    Phase Buffer (4KB)
                                         ↓
                                    Correlation Engine
                                         ↓
                                    Display Buffer → GPU
```

### Key Parameters
- `PHASE_BUFFER_SIZE`: 4096 samples (~93ms at 44.1kHz)
- `CORR_WINDOW`: 512 samples (~11.6ms) for correlation
- `DISPLAY_SAMPLES`: 2400 samples (screen width)
- `phase_smoothing`: 0.9 (0=no smoothing, 1=maximum smoothing)

### Advantages Over Simple Triggering
1. **Works with complex waveforms**: No need for clean zero crossings
2. **Frequency agnostic**: Automatically adapts to any frequency
3. **Noise resistant**: Correlation averages out random noise
4. **Multi-frequency capable**: Locks onto the dominant pattern

## Building and Running

```bash
# Build release version
cmake -B build-release -DCMAKE_BUILD_TYPE=Release .
cmake --build build-release --config Release

# Run the optimized visualizer
./build-release/visualizer_optimized
```

## Future Enhancements

1. **Adjustable smoothing**: Keyboard controls for phase smoothing factor
2. **Multiple reference windows**: Track multiple frequencies simultaneously
3. **FFT-based correlation**: Use frequency domain for faster correlation
4. **Adaptive window size**: Automatically adjust correlation window based on dominant frequency
5. **Lock indicators**: Visual indicators showing which frequency is locked

## Technical Notes

- The correlation is computed in the time domain for low latency
- Reference window updates are timed to avoid disrupting the lock
- Phase offset interpolation handles circular buffer wraparound
- All computations are done in the render thread to avoid blocking audio