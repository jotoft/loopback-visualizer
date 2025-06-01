#version 330 core

uniform vec2 resolution;
uniform float trigger_level;
uniform bool phase_lock_enabled;
uniform bool ghost_trails_enabled;
uniform int ghost_trail_count;  // How many trails to show
uniform float ghost_alpha_decay; // How much each trail fades

layout(std140) uniform SamplesBlock {
    float samples[2400];
};

// Previous frame samples for ghost trails (circular buffer)
uniform float ghost_samples[16 * 2400];  // Up to 16 trails

out vec4 FragColor;

// Function to draw a single waveform
float drawWaveform(vec2 uv, float y_audio, int offset) {
    float sample_idx_f = uv.x * 2399.0;
    int idx = int(floor(sample_idx_f));
    float fract = fract(sample_idx_f);
    
    if (idx < 0 || idx >= 2399) return 0.0;
    
    // Get samples from the ghost buffer
    float y0 = ghost_samples[offset + idx];
    float y1 = ghost_samples[offset + idx + 1];
    
    vec2 p0 = vec2(float(idx) / 2399.0, y0);
    vec2 p1 = vec2(float(idx + 1) / 2399.0, y1);
    vec2 p = vec2(uv.x, y_audio);
    
    vec2 v = p1 - p0;
    vec2 w = p - p0;
    
    float t = clamp(dot(w, v) / dot(v, v), 0.0, 1.0);
    vec2 closest = p0 + t * v;
    
    float dist = length(p - closest) * resolution.y;
    return 1.0 - smoothstep(0.0, 1.5, dist);
}

void main() {
    vec2 uv = gl_FragCoord.xy / resolution;
    float y_audio = (uv.y - 0.5) * 2.0;
    
    vec4 finalColor = vec4(0.0);
    
    // Draw ghost trails if enabled
    if (ghost_trails_enabled && ghost_trail_count > 0) {
        // Draw oldest trails first
        for (int i = ghost_trail_count - 1; i >= 0; i--) {
            float line = drawWaveform(uv, y_audio, i * 2400);
            
            if (line > 0.0) {
                // Calculate fade based on age
                float age = float(i) / float(ghost_trail_count);
                float alpha = pow(1.0 - age, 2.0) * ghost_alpha_decay;
                
                // Color shift - older trails become more blue/purple
                vec3 color = vec3(
                    0.5 - 0.3 * age,
                    0.8 - 0.3 * age,
                    0.9 + 0.1 * age
                );
                
                // Blend with existing color
                vec4 trailColor = vec4(color * line, line * alpha);
                finalColor = finalColor + trailColor * (1.0 - finalColor.a);
            }
        }
    }
    
    // Draw current waveform (always on top)
    float sample_idx_f = uv.x * 2399.0;
    int idx = int(floor(sample_idx_f));
    float fract = fract(sample_idx_f);
    
    if (idx >= 0 && idx < 2399) {
        float y0 = samples[idx];
        float y1 = samples[idx + 1];
        
        vec2 p0 = vec2(float(idx) / 2399.0, y0);
        vec2 p1 = vec2(float(idx + 1) / 2399.0, y1);
        vec2 p = vec2(uv.x, y_audio);
        
        vec2 v = p1 - p0;
        vec2 w = p - p0;
        
        float t = clamp(dot(w, v) / dot(v, v), 0.0, 1.0);
        vec2 closest = p0 + t * v;
        
        float dist = length(p - closest) * resolution.y;
        float line = 1.0 - smoothstep(0.0, 1.5, dist);
        
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
            
            // Current waveform overwrites trails
            finalColor = vec4(color * line, line);
        }
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