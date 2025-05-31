# Loopback Visualizer - Project Context

## Project Overview
Real-time audio oscilloscope that captures system audio output and visualizes it using OpenGL shaders. Cross-platform support for Windows (WASAPI) and Linux (PulseAudio).

## Language & Standards
- **C++23** - Modern C++ with latest language features
- **Custom monadic types** - Rust-inspired Result<T,E> and Option<T> for clean error handling

## Architecture
- **Audio Layer**: `audio_loopback/` - Cross-platform audio capture with callback interface
- **Processing**: `audio_filters/` - Sample filtering utilities
- **Visualization**: OpenGL shaders with GPU-based rendering and circular buffer management
- **Core Utilities**: `core/` - Custom Result/Option types and utility functions
- **Main Loop**: `main.cpp` - Coordinates audio capture, processing, and rendering

## Build System
**CMake with vendored dependencies**: All dependencies are included in `3rdparty/` for predictable builds.

### Prerequisites
- CMake 3.13 or higher
- C++23 compatible compiler

### Build Commands
```bash
# Debug build (from project root)
cmake -B build .
cmake --build build

# Release build  
cmake -B build-release -DCMAKE_BUILD_TYPE=Release .
cmake --build build-release --config Release

# Using Task runner (recommended)
task test    # Run all tests including Result/Option tests
task run     # Build and run debug visualizer
task release # Build release version
```

### Vendored Dependencies (3rdparty/)
- **GLAD**: OpenGL function loader
- **GLFW**: Cross-platform windowing  
- **Catch2**: Testing framework

## Key Files
- `main.cpp` - Main application with OpenGL setup and audio callback
- `soundwave.glsl` - Fragment shader for waveform rendering
- `audio_loopback/` - Audio capture abstraction layer
- `audio_filters/` - Signal processing utilities
- `core/result.h` - Monadic Result<T,E> type for error handling
- `core/option.h` - Monadic Option<T> type for nullable values
- `Taskfile.yml` - Task runner configuration for cross-platform builds

## Coding Guidelines

### Error Handling & Nullable Values
**MANDATORY**: Use custom monadic types for clean, safe code:

#### Result<T, E> - For operations that might fail
```cpp
#include "core/result.h"
using namespace core;

// For operations that can fail
auto divide(int a, int b) -> Result<int, std::string> {
    if (b == 0) return Result<int, std::string>::Err("Division by zero");
    return Result<int, std::string>::Ok(a / b);
}

// Monadic chaining
auto result = divide(10, 2)
    .map([](int x) { return x * 2; })
    .and_then([](int x) { return divide(x, 3); });
```

#### Option<T> - For values that might not exist
```cpp
#include "core/option.h"
using namespace core;

// For nullable/optional values
auto find_device(const std::string& name) -> Option<AudioDevice> {
    // Search logic...
    if (found) return Some(device);
    return None<AudioDevice>();
}

// Safe chaining without null checks
auto device_sample_rate = find_device("default")
    .map([](const auto& dev) { return dev.sample_rate; })
    .unwrap_or(44100);
```

#### Rules:
1. **SHALL use Result<T,E>** for any operation that might fail (file I/O, parsing, device operations)
2. **SHALL use Option<T>** for values that might not be set or found
3. **SHALL NOT use raw pointers** for nullable values - use Option<T&> or Option<std::unique_ptr<T>>
4. **SHALL use monadic chaining** (.map, .and_then, .filter) instead of nested if statements
5. **SHALL handle all error cases** - no ignored Result values

## Code Patterns
- **C++23** with modern features and custom monadic types
- Mutex-protected circular buffer for audio data
- Phase-locking algorithm for stable waveform display
- GPU uniform buffer objects for efficient sample transfer
- Comprehensive error handling with Result/Option types

## Development Notes
- Audio callback runs on separate thread - use mutex for shared data
- Sample buffer size: 2.4M samples (BUFFER_LENGTH)
- Display window: 2400 samples
- All error-prone operations must return Result<T,E>
- All potentially missing values must use Option<T>

## Testing
```bash
task test    # Run all tests including Result/Option tests
task run     # Build and run visualizer
```