# Adaptive Phase-Locked Audio Visualization System

## Overview

A real-time audio visualization system that automatically discovers and tracks multiple instruments/frequencies in a music stream, maintaining phase coherence for smooth visualization while adapting to changing musical content.

## Core Concepts

### 1. **Adaptive Frequency Discovery**
- Continuously analyze incoming audio spectrum via FFT
- Automatically detect prominent frequencies (instruments/notes)
- No prior knowledge of what instruments are playing required
- Adapt to genres from solo piano to full orchestra

### 2. **Phase-Locked Loop (PLL) Tracking**
- Each discovered frequency gets its own PLL tracker
- Maintains phase coherence for smooth, non-jumping visualizations
- Tracks frequency drift (e.g., vibrato, pitch bends)
- Provides lock quality metrics for visualization

### 3. **Intelligent Persistence**
- Don't immediately drop trackers when signal disappears
- Confidence scoring system for each tracker
- Time-based hysteresis prevents flickering
- Graceful fade-in/fade-out of visual elements

### 4. **Lock-Free Architecture**
- Audio thread only captures raw samples
- All analysis happens in render thread
- No mutex contention or audio dropouts
- Multiple visualization clients can read simultaneously

## System Architecture

```
┌─────────────────┐     ┌──────────────────┐     ┌─────────────────┐
│   Audio Input   │────►│  Lock-Free Ring  │────►│ Adaptive Phase  │
│  (PulseAudio)   │     │     Buffer       │     │    Tracker      │
└─────────────────┘     └──────────────────┘     └────────┬────────┘
                                                           │
                                ┌──────────────────────────┼────────┐
                                │                          ▼        │
                        ┌───────┴────────┐        ┌────────────────┐
                        │ FFT Frequency  │        │  PLL Array     │
                        │   Discovery    │        │ (16 trackers)  │
                        └───────┬────────┘        └────────────────┘
                                │                          │
                                └──────────┬───────────────┘
                                           ▼
                                   ┌───────────────┐
                                   │ Visualization │
                                   │     Data      │
                                   └───────────────┘
```

## Key Components

### FrequencyTracker
- **Frequency**: Current tracked frequency (Hz)
- **Phase**: Current phase (0-2π) for smooth visualization
- **Amplitude**: Signal strength at this frequency
- **Confidence**: How sure we are this is a real instrument (0-1)
- **Lock Quality**: How well the PLL is tracking (0-1)
- **Persistence Timer**: Time since last strong signal

### AdaptivePhaseTracker
- **Tracker Pool**: Up to 16 simultaneous frequency trackers
- **FFT Analysis**: 4096-point FFT for frequency discovery
- **Peak Detection**: Find local maxima in spectrum
- **Tracker Assignment**: Match peaks to existing trackers or create new
- **Cleanup**: Remove dead trackers after timeout

### Visualization Data Flow
1. Raw audio → Ring buffer (lock-free write)
2. Ring buffer → Phase tracker (lock-free read)
3. Phase tracker → Visualization buffer (lock-free write)
4. Visualization buffer → Renderer (lock-free read)

## Algorithm Details

### Frequency Discovery (every ~23ms)
```
1. Perform FFT on recent audio samples
2. Find spectral peaks above noise floor
3. For each peak:
   - Find closest existing tracker
   - If distance > 5 Hz, create new tracker
   - If close match, reinforce existing tracker
4. Sort peaks by magnitude, keep top 8
```

### PLL Update (every sample)
```
1. Generate reference sinusoid at tracked frequency
2. Multiply input by reference (demodulation)
3. Extract phase error via atan2(Q, I)
4. Update frequency estimate (PI controller)
5. Update phase accumulator
6. Update amplitude estimate (moving average)
7. Update confidence based on signal strength
```

### Persistence Logic
```
- Signal present: Increase confidence (up to 1.0)
- Signal absent: Slowly decay confidence
- Remove tracker when:
  - No signal for 2+ seconds AND
  - Confidence < 0.05
```

## Visualization Features

### Per-Tracker Data
- **Frequency**: For color mapping or Y-position
- **Phase**: For waveform position (X-axis)
- **Amplitude**: For brightness or size
- **Confidence**: For opacity (fade in/out)
- **Lock Quality**: For visual effects (blur when unlocked)

### Visual Mapping Ideas
1. **Frequency → Color**: Rainbow spectrum or musical intervals
2. **Phase → Position**: Horizontal wave position
3. **Amplitude → Brightness**: Louder = brighter
4. **Confidence → Opacity**: Smooth appearance/disappearance
5. **Lock Quality → Sharpness**: Locked = sharp, unlocked = fuzzy

### Multi-Instrument Visualization
- Each tracker gets a "swim lane" by frequency
- Vertical position = pitch (low to high)
- Horizontal movement = phase progression
- Multiple instruments create layered visualization
- Harmonics naturally group together

## Implementation Priorities

### Phase 1: Core Tracking
- [x] Lock-free ring buffer
- [ ] Basic FFT frequency discovery
- [ ] Single PLL tracker
- [ ] Simple amplitude visualization

### Phase 2: Adaptive System
- [ ] Multiple simultaneous trackers
- [ ] Confidence scoring
- [ ] Persistence logic
- [ ] Smooth transitions

### Phase 3: Advanced Features
- [ ] Harmonic relationship detection
- [ ] Instrument classification
- [ ] Beat/tempo detection
- [ ] Musical key detection

### Phase 4: Visualization Polish
- [ ] GPU-accelerated rendering
- [ ] Multiple visualization modes
- [ ] Color themes
- [ ] Configuration UI

## Performance Considerations

### Memory Usage
- Ring buffers: ~8MB total
- FFT workspace: ~64KB
- Tracker array: ~1KB
- Negligible compared to GPU buffers

### CPU Usage
- Audio thread: <1% (just copies)
- FFT: ~5% (every 23ms)
- PLLs: ~2% (16 trackers)
- Rendering: Depends on GPU

### Latency
- Audio capture: ~10ms
- FFT window: ~23ms
- PLL lock time: ~100ms
- Total visual latency: ~50-150ms

## Future Enhancements

1. **Machine Learning**
   - Instrument recognition
   - Genre classification
   - Predictive tracking

2. **Music Theory**
   - Chord detection
   - Scale/key analysis
   - Beat grid alignment

3. **Interactive Features**
   - Click to isolate frequency
   - Record specific instruments
   - MIDI output generation

4. **Visualization Modes**
   - 3D frequency landscape
   - Particle systems
   - Music notation display