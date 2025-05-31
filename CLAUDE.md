# Loopback Visualizer - Project Context

## Project Overview
Real-time audio oscilloscope that captures system audio output and visualizes it using OpenGL shaders. Cross-platform support for Windows (WASAPI) and Linux (PulseAudio).

## Architecture
- **Audio Layer**: `audio_loopback/` - Cross-platform audio capture with callback interface
- **Processing**: `audio_filters/` + metaFFT - Sample filtering and frequency analysis  
- **Visualization**: OpenGL shaders with GPU-based rendering and circular buffer management
- **Main Loop**: `main.cpp` - Coordinates audio capture, processing, and rendering

## Build System
```bash
# Debug build
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make

# Release build  
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

## Key Files
- `main.cpp` - Main application with OpenGL setup and audio callback
- `soundwave.glsl` - Fragment shader for waveform rendering
- `audio_loopback/` - Audio capture abstraction layer
- `audio_filters/` - Signal processing utilities

## Code Patterns
- Uses C++14 with some C-style arrays for performance
- Mutex-protected circular buffer for audio data
- Phase-locking algorithm for stable waveform display
- GPU uniform buffer objects for efficient sample transfer

## Development Notes
- Audio callback runs on separate thread - use mutex for shared data
- Sample buffer size: 2.4M samples (BUFFER_LENGTH)
- Display window: 2400 samples
- Hardcoded constants throughout codebase (improvement opportunity)
- Some experimental code (FFT, filtering) commented out

## Testing
Run `./visualizer` and play audio to see waveform visualization.
Use `./glfwmaintest` for basic OpenGL testing.