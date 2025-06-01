#version 330 core

uniform vec2 resolution;
uniform float trigger_level;
uniform bool phase_lock_enabled;
uniform bool ghost_trails_enabled;
uniform int trail_count;  // Number of ghost trails to render
uniform float time;

// Multiple sample buffers for ghost trails
layout(std140) uniform SamplesBlock {
    float samples[2400];
};

// Ghost trail samples (up to 8 previous frames)
layout(std140) uniform TrailsBlock {
    float trail_samples[8][2400];  // 8 trails x 2400 samples each
};

out vec4 FragColor;

// Function to draw a waveform
float drawWaveform(vec2 uv, float y_audio, float samples[2400], out float dist) {
    float sample_idx_f = uv.x * 2399.0;
    int idx = int(floor(sample_idx_f));
    float fract = fract(sample_idx_f);
    
    if (idx < 0 || idx >= 2399) {
        dist = 1000.0;
        return 0.0;
    }
    
    float y0 = samples[idx];
    float y1 = samples[idx + 1];
    float sample_value = mix(y0, y1, fract);
    
    vec2 p0 = vec2(float(idx) / 2399.0, y0);
    vec2 p1 = vec2(float(idx + 1) / 2399.0, y1);
    vec2 p = vec2(uv.x, y_audio);
    
    vec2 v = p1 - p0;
    vec2 w = p - p0;
    
    float t = clamp(dot(w, v) / dot(v, v), 0.0, 1.0);
    vec2 closest = p0 + t * v;
    
    dist = length(p - closest) * resolution.y;
    return 1.0 - smoothstep(0.0, 1.5, dist);
}

void main() {
    vec2 uv = gl_FragCoord.xy / resolution;
    float y_audio = (uv.y - 0.5) * 2.0;
    
    vec4 finalColor = vec4(0.0);
    
    // Draw ghost trails first (older waveforms)
    if (ghost_trails_enabled) {
        for (int i = trail_count - 1; i >= 0; i--) {
            float dist;
            float line = drawWaveform(uv, y_audio, trail_samples[i], dist);
            
            if (line > 0.0) {
                // Calculate fade based on age
                float age = float(i + 1) / float(trail_count);
                float alpha = (1.0 - age) * 0.5;  // Fade from 0.5 to 0
                
                // Color shift based on age - older trails shift toward purple/blue
                vec3 trailColor = vec3(
                    0.5 + 0.5 * (1.0 - age),
                    0.5 + 0.5 * cos(age * 3.14159),
                    0.8 + 0.2 * age
                );
                
                // Accumulate color
                vec4 trailContrib = vec4(trailColor * line * alpha, line * alpha);
                finalColor = finalColor + trailContrib * (1.0 - finalColor.a);
            }
        }
    }
    
    // Draw current waveform on top
    float dist;
    float line = drawWaveform(uv, y_audio, samples, dist);
    
    if (line > 0.0) {
        vec3 color = vec3(0.0, 1.0, 0.9);
        
        // Phase lock color coding
        if (phase_lock_enabled) {
            if (trigger_level > 0.7) {
                color = mix(color, vec3(0.0, 1.0, 0.0), 0.3);
            } else if (trigger_level > 0.5) {
                color = mix(color, vec3(1.0, 1.0, 0.0), 0.3);
            } else {
                color = mix(color, vec3(1.0, 0.0, 0.0), 0.3);
            }
        }
        
        // Current waveform is fully opaque
        finalColor = vec4(color * line, line);
    }
    
    // Correlation bar
    if (phase_lock_enabled && uv.y < 0.02) {
        if (uv.x < trigger_level) {
            vec3 barColor = vec3(0.0, trigger_level, 1.0 - trigger_level);
            finalColor = vec4(barColor, 1.0);
        }
    }
    
    FragColor = finalColor;
}