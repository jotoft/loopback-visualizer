# Loopback Visualizer

Real-time audio oscilloscope visualization using OpenGL that captures audio from your system's output device and renders it as a smooth waveform display. Works with any audio playback source.

## Features

- **Cross-platform support**: Windows (WASAPI) and Linux (PulseAudio) backends
- **Real-time visualization**: GPU-accelerated waveform rendering with phase-locked display
- **System audio capture**: Monitors default output device (loopback recording)
- **Smooth rendering**: 4x sample interpolation and anti-aliased line drawing
- **Audio processing**: Built-in filtering and FFT capabilities via metaFFT

## Requirements

- CMake 3.13+
- C++14 compiler
- OpenGL 3.3+ support
- Audio system: WASAPI (Windows) or PulseAudio (Linux)

## Building

```bash
mkdir build && cd build
cmake ..
make
```

## Running

```bash
./visualizer
```

The visualizer will automatically detect and use your system's default audio output device.

## Limitations

- Does not work with applications using exclusive audio mode
- Requires audio playback to be active for visualization

Note: The GIF below doesn't show the full fluidity of the actual application
![](visualization.gif)
