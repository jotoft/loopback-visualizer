# Multi-Template Phase Lock Design

## Overview
The multi-template system allows simultaneous tracking of different frequency bands or signal characteristics, each with its own phase-locked reference. Templates blend based on correlation strength, creating a rich, layered visualization.

## Architecture

### Core Components

```cpp
struct TemplateConfig {
    // Frequency band
    float low_freq = 20.0f;
    float high_freq = 20000.0f;
    
    // Phase lock settings
    float correlation_threshold = 0.15f;
    int window_size = 512;
    
    // Visual settings
    vec3 color = {0.0f, 1.0f, 0.9f};
    float base_alpha = 1.0f;
    
    // Dynamics
    float ema_alpha = 0.099f;        // Template update rate
    float fade_attack = 0.01f;       // How fast to fade in
    float fade_release = 0.05f;      // How fast to fade out
};

class MultiTemplateAnalyzer {
    std::vector<PhaseLockAnalyzer> analyzers_;
    std::vector<SimpleFilters> filters_;
    std::vector<TemplateConfig> configs_;
    std::vector<float> current_correlations_;
    std::vector<float> display_alphas_;  // Smoothed alpha values
};
```

### Template Examples

#### Template 1: Bass/Rhythm (20-200 Hz)
- Large window (1024 samples) for low frequencies
- Slow EMA (0.05) for stable bass patterns
- Red color theme
- Slow fade (captures sustained bass)

#### Template 2: Mids/Melody (200-4000 Hz)
- Medium window (512 samples)
- Medium EMA (0.1) for melodic tracking
- Cyan color theme
- Medium fade (responsive to changes)

## Visualization Strategies

### 1. Additive Blending
```glsl
vec4 final_color = vec4(0);
for (int i = 0; i < num_templates; i++) {
    float alpha = template_alpha[i] * correlation_strength[i];
    vec4 template_color = vec4(template_colors[i], alpha);
    final_color += template_color;
}
```

### 2. Layered Display
- Bass template at y = -0.5
- Mid template at y = 0.0  
- High template at y = +0.5
- Alpha indicates correlation strength

### 3. Dominant Template
- Only show template with highest correlation
- Smooth morphing between templates
- Color interpolation during transitions

## Ghost Trail Integration

### Shared Trail Pool
- All templates write to same trail buffer
- Trail color = weighted average of active templates
- Trail alpha = max correlation across templates

### Per-Template Trails
- Each template maintains own trail history
- Composite trails in shader
- More memory but richer visualization

## Fade Dynamics

### Correlation-Based Fading
```cpp
// For each template
float target_alpha = (correlation > threshold) ? 1.0f : 0.0f;

// Apply attack/release envelope
if (target_alpha > current_alpha) {
    current_alpha += (target_alpha - current_alpha) * fade_attack;
} else {
    current_alpha += (target_alpha - current_alpha) * fade_release;
}

// Scale by correlation quality
display_alpha = current_alpha * (correlation - threshold) / (1.0f - threshold);
```

### Time-Constant Matching
- Slow EMA → Slow fade (bass frequencies)
- Fast EMA → Fast fade (transients)
- Prevents jarring transitions

## Implementation Plan

### Phase 1: Dual Template MVP
1. Create MultiTemplateAnalyzer class
2. Implement two templates (bass + mids)
3. Basic additive blending in shader
4. Shared ghost trails

### Phase 2: Enhanced Blending
1. Smooth fade envelopes
2. Color interpolation
3. Correlation-weighted blending
4. Template switching UI

### Phase 3: Advanced Features
1. Dynamic template creation
2. Vertical separation display
3. Per-template ghost trails
4. Template morphing

## Performance Considerations

### CPU Side
- Run filters in parallel
- Share FFT results between templates
- Batch correlation computations

### GPU Side  
- Pack template data in uniform buffers
- Minimize state changes
- Use instancing for multiple templates

## User Interface

### Template Control Panel
```
[Template 1: Bass]
Frequency: 20-200 Hz
Window: 1024
Threshold: 0.15
Color: [====] Red
Alpha: 0.8
Status: LOCKED (0.78)

[Template 2: Mids]  
Frequency: 200-4000 Hz
Window: 512
Threshold: 0.15  
Color: [====] Cyan
Alpha: 0.6
Status: SEARCHING (0.12)
```

### Global Controls
- Template blend mode (Add/Layer/Dominant)
- Master correlation sensitivity
- Trail sharing mode
- Template preset loader