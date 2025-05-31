#version 330 core

uniform vec2 resolution;  // Add resolution uniform

layout(std140) uniform SamplesBlock {
    float samples[2400];
};

out vec4 FragColor;

void main() {
    // Normalize coordinates to [0,1]
    vec2 uv = gl_FragCoord.xy / resolution;
    
    // Map X to sample index (0 to 2399)
    float sample_idx_f = uv.x * 2399.0;
    int idx0 = int(floor(sample_idx_f));
    int idx1 = min(idx0 + 1, 2399);
    float fract = fract(sample_idx_f);
    
    // Get samples and interpolate
    float sample0 = samples[idx0];
    float sample1 = samples[idx1];
    float sample_value = mix(sample0, sample1, fract);
    
    // Map Y to [-1, 1] range for audio
    float y_audio = (uv.y - 0.5) * 2.0;
    
    // Distance from the waveform
    float dist = abs(y_audio - sample_value);
    
    // Thin line
    float line = 1.0 - smoothstep(0.0, 0.004, dist);
    
    // Color
    vec3 color = vec3(0.0, 1.0, 0.9);
    
    FragColor = vec4(color * line, line);
}