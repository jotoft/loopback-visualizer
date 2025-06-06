# Loopback Visualizer

Real-time audio oscilloscope visualization using OpenGL that captures audio from your system's output device and renders it as a smooth waveform display. Works with any audio playback source.

## Features

- **Cross-platform support**: Windows (WASAPI) and Linux (PulseAudio) backends
- **Real-time visualization**: GPU-accelerated waveform rendering at 240 FPS
- **Cross-correlation phase locking**: Advanced oscilloscope-style stable waveform display
- **Ultra-low latency**: ~5ms in normal mode, ~10-15ms with phase lock enabled
- **System audio capture**: Monitors default output device (loopback recording)
- **Input switching**: Toggle between system audio and microphone input
- **Smooth rendering**: Anti-aliased line drawing with OpenGL shaders
- **Modern C++**: Uses C++23 with Rust-inspired Result/Option monadic error handling
- **Lock-free audio**: Zero-copy audio capture with built-in ring buffer
- **Test-driven development**: Comprehensive test suite using Catch2

## Requirements

- CMake 3.16+
- C++23 compiler (GCC 13+, Clang 16+, MSVC 19.35+)
- OpenGL 3.3+ support
- Audio system: WASAPI (Windows) or PulseAudio (Linux)
- [vcpkg](https://vcpkg.io) package manager for dependencies
- [Task](https://taskfile.dev) (recommended for simplified build commands)

## Building

### Prerequisites

**Install vcpkg**: 
```bash
# Linux/macOS
git clone https://github.com/Microsoft/vcpkg.git ~/.local/share/vcpkg
~/.local/share/vcpkg/bootstrap-vcpkg.sh
export VCPKG_ROOT=$HOME/.local/share/vcpkg

# Windows  
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
C:\vcpkg\bootstrap-vcpkg.bat
set VCPKG_ROOT=C:\vcpkg
```

**Install Task** (optional but recommended):
- Arch Linux: `sudo pacman -S go-task` (binary is `go-task`)
- Install script: `sh -c "$(curl --location https://taskfile.dev/install.sh)" -- -d`
- Other platforms: https://taskfile.dev/installation/

### Using Task (recommended)
```bash
go-task run         # Build and run debug version
go-task release     # Build and run optimized release version (recommended)
go-task build       # Just build debug version
go-task test        # Run test suite
go-task clean       # Clean build artifacts
```

### Manual build
```bash
# Ensure VCPKG_ROOT is set and VCPKG_FEATURE_FLAGS includes manifests
export VCPKG_FEATURE_FLAGS=manifests

# Debug build
VCPKG_FEATURE_FLAGS=manifests cmake -B build -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" .
cmake --build build
./build/visualizer

# Release build  
VCPKG_FEATURE_FLAGS=manifests cmake -B build-release -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE="$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" .
cmake --build build-release --config Release
./build-release/visualizer
```

### Dependencies

The project uses vcpkg manifest mode with `vcpkg.json` to automatically manage dependencies:
- **GLAD**: OpenGL function loader
- **GLFW**: Cross-platform windowing and input
- **Catch2**: Testing framework

Dependencies are automatically installed by vcpkg during the CMake configure step.

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

### Controls

- **P**: Toggle phase lock ON/OFF (stabilizes waveform display)
- **I**: Switch between system audio (loopback) and microphone input
- **ESC/Q**: Quit application

### Phase Locking

The visualizer includes advanced cross-correlation based phase locking that makes the waveform appear stationary, similar to a traditional oscilloscope:

- **Green waveform**: Excellent phase lock (correlation > 0.7)
- **Yellow waveform**: Moderate lock (correlation 0.5-0.7)
- **Red waveform**: Poor lock (correlation < 0.5)
- **Blue bar**: Shows correlation strength at bottom of screen

See [docs/cross-correlation-phase-lock.md](docs/cross-correlation-phase-lock.md) for technical details.

## Limitations

- Does not work with applications using exclusive audio mode
- Requires audio playback to be active for visualization
- Performance may vary based on audio buffer configuration

Note: The GIF below doesn't show the full fluidity of the actual application
![](visualization.gif)
