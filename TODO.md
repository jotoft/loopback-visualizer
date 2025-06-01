# TODO List

## Current Focus - Multi-Template Phase Lock System

### Phase 1: Dual Template Proof of Concept (In Progress)
- [ ] **Implement MultiTemplateAnalyzer class** - Manage multiple phase lock analyzers
  - [ ] Support for 2 concurrent templates with different filter configs
  - [ ] Independent correlation tracking per template
  - [ ] Configurable frequency bands (e.g., bass 20-200Hz, mids 200-4kHz)
  
- [ ] **Template Blending & Visualization**
  - [ ] Alpha blending based on correlation strength
  - [ ] Color assignment per template (e.g., red for bass, cyan for mids)
  - [ ] Smooth fade in/out when correlation is gained/lost
  - [ ] Shared ghost trail buffer across templates

- [ ] **Dynamic Responsiveness**
  - [ ] Multiple EMA filters per band with different time constants
  - [ ] Fade speed proportional to EMA responsiveness (slow filter = slow fade)
  - [ ] Visual indication of which template is "dominant"

### Phase 2: Enhanced Multi-Template Features
- [ ] **Spectral Layer Visualization**
  - [ ] Vertical separation of frequency bands
  - [ ] Option to view templates separately or combined
  - [ ] Spectral "heat map" showing which frequencies are locked

- [ ] **Adaptive Template Management**
  - [ ] Automatic template creation based on spectral content
  - [ ] Template "competition" - strongest correlation wins display priority
  - [ ] Template morphing for smooth transitions

- [ ] **Advanced Ghost Trails**
  - [ ] Per-template trail colors
  - [ ] Trail intensity based on historical correlation strength
  - [ ] "Harmonic trails" showing frequency relationships

## Completed Features ✓

### Core Visualization
- [x] Real-time audio capture (PulseAudio/WASAPI)
- [x] Phase-locked averaging with cross-correlation
- [x] Reference waveform accumulation (EMA and Accumulator modes)
- [x] Cubic interpolation for smooth waveform display
- [x] Ghost trails with correlation-based fade dynamics
- [x] 240 FPS rendering with minimal latency

### Signal Processing
- [x] Custom FFT implementation (Cooley-Tukey)
- [x] Frequency analyzer with peak detection
- [x] Band-pass filtering for correlation
- [x] Simple filters (high-pass, low-pass, de-esser)
- [x] Filters affect phase locking for accurate correlation

### User Interface
- [x] ImGui integration with HiDPI support
- [x] Real-time parameter adjustment
- [x] Audio device selection
- [x] Visual correlation feedback
- [x] Spectrum analyzer display
- [x] Filter control panel

## Future Ideas

### Visualization Enhancements
- [ ] 3D waveform with depth representing correlation confidence
- [ ] Particle effects for transients
- [ ] Music-reactive color schemes
- [ ] Lissajous/X-Y mode for stereo analysis

### Analysis Features
- [ ] Beat detection and BPM tracking
- [ ] Note/pitch detection with musical notation
- [ ] Harmonic analysis and chord recognition
- [ ] Transient detection and visualization

### Performance & Architecture
- [ ] GPU compute shaders for correlation
- [ ] Multi-threaded template processing
- [ ] Lock-free audio pipeline optimization
- [ ] Plugin architecture for custom visualizations

### Export & Integration
- [ ] Video export of visualizations
- [ ] VST/AU plugin version
- [ ] MIDI output from detected notes
- [ ] OSC integration for VJing

## Technical Debt
- [ ] Refactor main.cpp into smaller modules
- [ ] Add comprehensive unit tests for signal processing
- [ ] Document the phase-locking algorithm
- [ ] Profile and optimize memory usage
- [ ] Add configuration file support (YAML/TOML)

## Bug Fixes
- [ ] Handle audio device disconnection gracefully
- [ ] Fix potential race conditions in ring buffer
- [ ] Improve error messages for shader compilation
- [ ] Add bounds checking for all array accesses