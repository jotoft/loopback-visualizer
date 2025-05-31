# TODO List

## High Priority - Core Functionality
- [x] **Fix white window issue** - ~~Application shows only white window instead of waveform visualization~~
  - Fixed by copying shader files to build directory in Taskfile
- [x] Debug OpenGL rendering pipeline (shaders, vertex data, uniforms) - Working correctly
- [ ] **Fix live audio capture** - Currently shows static waveform, need live data flow
- [ ] **Fix graceful shutdown** - PulseAudio thread cleanup on window close
- [ ] Verify audio data flow from PulseAudio to GPU buffers

## Medium Priority - Dependencies
- [ ] **Replace metaFFT with custom implementation** - Remove external FFT dependency
  - Current metaFFT usage is minimal and commented out in main.cpp
  - Implement basic radix-2 FFT for frequency domain analysis
  - Consider if FFT is needed at all for waveform visualization

## Low Priority - Improvements
- [ ] Modernize C++ code style (move away from C-style arrays)
- [ ] Make hardcoded constants configurable
- [ ] Add better error handling for audio failures
- [ ] Optimize GPU buffer updates
- [ ] Add configuration file support

## Build System
- [x] Add Taskfile for npm-style build commands
- [x] Fix cross-platform building
- [x] Update documentation