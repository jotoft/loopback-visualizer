# Loopback Visualizer

Real-time audio oscilloscope visualization using OpenGL that captures audio from your system's output device and renders it as a smooth waveform display. Works with any audio playback source.

## Features

- **Cross-platform support**: Windows (WASAPI) and Linux (PulseAudio) backends
- **Real-time visualization**: GPU-accelerated waveform rendering with phase-locked display
- **System audio capture**: Monitors default output device (loopback recording)
- **Smooth rendering**: Anti-aliased line drawing with OpenGL shaders
- **Modern C++**: Uses C++23 with Rust-inspired Result/Option monadic error handling
- **Audio processing**: Built-in filtering capabilities with custom implementations
- **Test-driven development**: Comprehensive test suite using Catch2

## Requirements

- CMake 3.16+
- C++23 compiler (GCC 13+, Clang 16+, MSVC 19.35+)
- OpenGL 3.3+ support
- Audio system: WASAPI (Windows) or PulseAudio (Linux)
- [Task](https://taskfile.dev) (recommended for simplified build commands)

## Building

### Using Task (recommended)
Install Task first:
- Arch Linux: `sudo pacman -S go-task` (binary is `go-task`)
- Install script: `sh -c "$(curl --location https://taskfile.dev/install.sh)" -- -d`
- Other platforms: https://taskfile.dev/installation/

```bash
go-task run         # Build and run debug version
go-task run-release # Build and run optimized release version  
go-task build       # Just build debug version
go-task release     # Just build release version
go-task test        # Run test suite
go-task clean       # Clean build artifacts
```

### Manual build
```bash
# Initialize submodules first
git submodule update --init --recursive

# Debug build
mkdir build && cd build
cmake ..
make
./visualizer

# Release build  
mkdir build-release && cd build-release
cmake -DCMAKE_BUILD_TYPE=Release ..
make
./visualizer
```

## Development

The project follows modern C++ practices with comprehensive error handling:

- **Result<T,E>**: Used for operations that can fail (file I/O, network, system calls)
- **Option<T>**: Used for nullable values and optional operations
- **Monadic chaining**: Enables clean, functional-style error handling
- **Zero-cost abstractions**: Release builds have minimal overhead

See `CLAUDE.md` for detailed coding guidelines and `REVIEW.md` for current compliance status.

### Running Tests
```bash
go-task test  # Run all tests
# Or manually: ./build/tests/tests
```

## Usage

The visualizer will automatically detect and use your system's default audio output device. On Linux, it uses PulseAudio's monitor device (`@DEFAULT_MONITOR@`).

## Limitations

- Does not work with applications using exclusive audio mode
- Requires audio playback to be active for visualization
- Performance may vary based on audio buffer configuration

Note: The GIF below doesn't show the full fluidity of the actual application
![](visualization.gif)
