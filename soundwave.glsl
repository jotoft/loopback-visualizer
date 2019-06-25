#version 330
uniform int current_sample;

layout(std140) uniform SamplesBlock
{
    float samples[2400];
};

float plot(vec2 st, float pct){
    return  smoothstep( pct-10, pct, st.y) -
    smoothstep( pct, pct+10, st.y);
}

out vec4 FragColor;
void main() {

    vec2 st = gl_FragCoord.xy;
    int buffer_coord = int(floor(gl_FragCoord.x));
    int sample_loc = 2399-(buffer_coord);
    float sampval = samples[sample_loc];
    float val = plot(st, sampval*800);

    FragColor = vec4(vec3(val), 1.0);
    //FragColor = vec4(vec3((gl_FragCoord.x)/1200.0), 1.0);
}
