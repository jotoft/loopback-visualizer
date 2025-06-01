# Loopback Visualizer - Project Context

## Project Overview
Advanced real-time audio visualization using phase-locked averaging. Captures system audio and creates stable, beautiful waveform displays through cross-correlation and adaptive template matching. Features ghost trails, frequency analysis, and sophisticated filtering.

## Key Innovation: Phase-Locked Averaging
The visualizer uses cross-correlation to detect repeating patterns in audio, building a stable reference waveform that updates smoothly. This creates clean, jitter-free visualizations of complex audio signals.

## Language & Standards
- **C++23** - Modern C++ with latest language features
- **Custom monadic types** - Rust-inspired Result<T,E> and Option<T> for clean error handling
- **ImGui** - Immediate mode GUI for real-time controls

## Architecture

### Core Components
- **Audio Layer**: `audio_loopback/` - Lock-free audio capture with ring buffer
- **Visualization**: `src/visualization/` - Phase lock analysis, frequency analysis, filters
- **Shaders**: `shaders/` - GPU-accelerated waveform rendering
- **GUI**: ImGui integration with HiDPI support
- **Main Loop**: `src/main.cpp` - 240 FPS render loop with audio processing

### Key Classes
- **PhaseLockAnalyzer**: Cross-correlation based phase detection and reference building
- **FrequencyAnalyzer**: Real-time FFT with peak detection
- **SimpleFilters**: Biquad IIR filters (high-pass, low-pass, de-esser)
- **AudioCapture**: Platform-specific audio capture (PulseAudio/WASAPI)

## Current Features

### Visualization
- **Phase-locked display**: Stable waveform through cross-correlation
- **Reference modes**: EMA (exponential moving average) or Accumulator
- **Ghost trails**: Dynamic fade based on correlation strength
- **Cubic interpolation**: Smooth upsampling of reference waveform
- **Real-time filters**: Affect both display and phase locking

### Default Configuration
```cpp
// Phase Lock
- Mode: EMA with alpha 0.099
- Correlation threshold: 0.15
- Window size: 512 samples
- Display reference waveform by default

// Filters  
- High-pass: 90 Hz (removes mains hum)
- Resonance: 0.5

// Audio
- Default to system output (loopback)
```

## Build System

### Using Task (Recommended)
```bash
task run      # Build and run debug version
task release  # Build optimized version
task test     # Run all tests
task clean    # Clean build artifacts
```

### Direct CMake
```bash
cmake -B build .
cmake --build build
./build/visualizer
```

### Dependencies (Vendored in 3rdparty/)
- **GLAD**: OpenGL function loader
- **GLFW**: Cross-platform windowing
- **ImGui**: Immediate mode GUI (as git submodule)
- **Catch2**: Testing framework

## Development Workflow

### Adding Audio Processing
1. Filters must process audio BEFORE phase analysis
2. Use SimpleFilters as example for biquad implementations
3. Reset phase analyzer when filter settings change

### GUI Development
1. Add controls in main.cpp ImGui section
2. Use tooltips for parameter explanations
3. Group related controls in collapsible headers

### Shader Modifications
1. Shaders are copied to build directory
2. Uniforms for dynamic parameters
3. Use reference_mode flag for quality hints

## Current Direction: Multi-Template System

Working on supporting multiple phase-lock templates simultaneously:
- Different frequency bands with own templates
- Blend based on correlation strength
- Dynamic fade rates matching template responsiveness
- Shared ghost trail system

## Code Patterns

### Error Handling
```cpp
// Always use Result for fallible operations
auto result = audio::create_audio_capture(device);
if (result.is_err()) {
    return Result<Unit, std::string>::Err("Failed to create capture");
}

// Chain operations monadically
auto samples = read_samples()
    .and_then([](auto s) { return filter_samples(s); })
    .map([](auto s) { return normalize(s); });
```

### Real-time Constraints
- No allocations in audio callback
- Lock-free ring buffer for audio data
- Mutex only for non-critical paths
- Pre-allocate all buffers

### Phase Lock Algorithm
1. Maintain circular buffer of audio samples
2. Extract reference window from recent "good" correlation
3. Cross-correlate incoming audio with reference
4. Update read position based on best correlation offset
5. Blend new matches into reference (EMA or accumulator)
6. Reset reference if correlation stays poor

## Testing
- Unit tests for core algorithms in `tests/`
- Manual testing with different audio sources
- Performance profiling with Release builds

## Future Plans
See TODO.md for detailed roadmap. Key areas:
- Multi-template visualization
- GPU compute for correlation
- Advanced audio analysis (beat detection, pitch tracking)
- Plugin architecture