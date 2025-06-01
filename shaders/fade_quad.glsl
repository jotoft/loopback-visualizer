#version 330 core

uniform float fade_alpha;

out vec4 FragColor;

void main() {
    FragColor = vec4(0.0, 0.0, 0.0, fade_alpha);
}