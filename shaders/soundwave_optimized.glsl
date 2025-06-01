#version 330 core

uniform vec2 resolution;
uniform float trigger_level;  // Trigger level for phase lock display
uniform bool phase_lock_enabled;  // Whether phase lock is active
uniform float waveform_alpha = 1.0;  // Alpha value for ghost trails
uniform vec3 waveform_color = vec3(0.0, 1.0, 0.9);  // Color override for ghost trails
uniform bool reference_mode = false;  // Using reference waveform (better antialiasing)

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
    // Use wider smoothstep for reference mode (better antialiasing)
    float edge_width = reference_mode ? 2.0 : 1.5;
    float line = 1.0 - smoothstep(0.0, edge_width, dist);
    
    // Rainbow effect based on position and sample value
    float hue = uv.x * 2.0 + sample_value * 0.5;
    
    // Simple HSV to RGB conversion
    float h = mod(hue, 1.0) * 6.0;
    float c = 0.8; // saturation
    float x = c * (1.0 - abs(mod(h, 2.0) - 1.0));
    
    vec3 rgb;
    if (h < 1.0) rgb = vec3(c, x, 0.0);
    else if (h < 2.0) rgb = vec3(x, c, 0.0);
    else if (h < 3.0) rgb = vec3(0.0, c, x);
    else if (h < 4.0) rgb = vec3(0.0, x, c);
    else if (h < 5.0) rgb = vec3(x, 0.0, c);
    else rgb = vec3(c, 0.0, x);
    
    vec3 rainbow = rgb + vec3(0.2); // add brightness
    
    // Mix rainbow with base color
    vec3 color = mix(waveform_color, rainbow, 0.7);
    
    // Add correlation indicator when phase lock is enabled
    if (phase_lock_enabled) {
        // Change color based on correlation quality
        if (trigger_level > 0.7) {
            // Good lock - enhance cyan/blue
            color = mix(color, vec3(0.0, 0.8, 1.0), 0.4);
        } else if (trigger_level > 0.5) {
            // Moderate lock - enhance yellow/orange
            color = mix(color, vec3(1.0, 0.7, 0.0), 0.4);
        } else {
            // Poor lock - enhance red/purple
            color = mix(color, vec3(1.0, 0.0, 0.5), 0.4);
        }
        
        // Show correlation bar at bottom
//        if (uv.y < 0.02) {
//            float bar_progress = uv.x/10.0;
//            if (bar_progress < trigger_level) {
//                color = vec3(0.0, trigger_level, 1.0 - trigger_level);
//                line = 1.0;
//            }
//        }
    }
    
    FragColor = vec4(color * line, line * waveform_alpha);
}