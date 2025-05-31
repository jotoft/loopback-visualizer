#version 330 core

uniform vec2 resolution;

layout(std140) uniform SamplesBlock {
    float samples[2400];
};

out vec4 FragColor;

void main() {
    // Normalize coordinates to [0,1]
    vec2 uv = gl_FragCoord.xy / resolution;
    
    // Map X to sample index (0 to 2399)
    float sample_idx_f = uv.x * 2399.0;
    int idx = int(floor(sample_idx_f));
    float fract = fract(sample_idx_f);
    
    // Bounds check
    if (idx < 0 || idx >= 2399) {
        FragColor = vec4(0.0);
        return;
    }
    
    // Get current sample and neighbors for better line connectivity
    float y0 = samples[idx];
    float y1 = samples[idx + 1];
    
    // Linear interpolation for smooth line
    float sample_value = mix(y0, y1, fract);
    
    // Map Y to [-1, 1] range for audio
    float y_audio = (uv.y - 0.5) * 2.0;
    
    // For better line connectivity, check distance to line segment
    vec2 p0 = vec2(float(idx) / 2399.0, y0);
    vec2 p1 = vec2(float(idx + 1) / 2399.0, y1);
    vec2 p = vec2(uv.x, y_audio);
    
    // Vector from p0 to p1
    vec2 v = p1 - p0;
    // Vector from p0 to current point
    vec2 w = p - p0;
    
    // Project w onto v to find closest point on line segment
    float t = clamp(dot(w, v) / dot(v, v), 0.0, 1.0);
    vec2 closest = p0 + t * v;
    
    // Distance to line segment
    float dist = length(p - closest);
    
    // Convert distance to pixel space for consistent line width
    dist *= resolution.y;
    
    // Thin line with smooth anti-aliasing
    float line = 1.0 - smoothstep(0.0, 1.5, dist);
    
    // Color
    vec3 color = vec3(0.0, 1.0, 0.9);
    
    FragColor = vec4(color * line, line);
}