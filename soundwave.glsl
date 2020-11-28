#version 330
uniform int current_sample;

uniform float time;

layout(std140) uniform SamplesBlock
{
    vec4 samples[2400];
};


float plot(vec2 st, float pct){
    return  smoothstep( pct-0.006F, pct, st.y) -
    smoothstep( pct, pct+0.006F, st.y);
}

float calc_distance(float current_sample, float next_sample, vec2 st)
{
    vec2 point1 = vec2(0.0F, current_sample);
    //vec2 point2 = vec2(1.0F/400.0F, 0.0F);
    vec2 point2 = vec2(1.0F/400.0F, 0.0F);

    vec2 sample_line = point2 - point1;
    vec2 normalized = normalize(sample_line);
    vec2 rotated_normalized = vec2(normalized.y, -normalized.x);
    float distance = abs(dot(rotated_normalized, vec2(0.0F, current_sample-st.y)));
    return distance;
}


float calc_distance2(float current_sample, float next_sample, vec2 st)
{
    vec2 point1 = vec2(0.0F, current_sample);
    vec2 point2 = vec2(0.0F/400.0F, 0.0F);

    vec2 sample_line = point2 - point1;
    vec2 normalized = normalize(sample_line);
    vec2 rotated_normalized = vec2(normalized.y, -normalized.x);
    float distance = abs(dot(rotated_normalized, vec2(0.0F, current_sample-st.y)));
    return distance;
}

vec4 calc_pixel_values(int sample_loc, vec2 st)
{
    float sampval1 = samples[sample_loc].x;
    float sampval_next1 = samples[sample_loc+1].x;

    float sampval2 = samples[sample_loc].y;
    float sampval_next2 = samples[sample_loc+1].y;

    float sampval3 = samples[sample_loc].z;
    float sampval_next3 = samples[sample_loc+1].z;

    float distance1 = calc_distance(sampval1, sampval_next1, st);
    float distance2 = calc_distance(sampval2, sampval_next2, st);
    float distance3 = calc_distance(sampval3, sampval_next3, st);

    float val1 = smoothstep(0.003*2.0, 0.0001*2.0, distance1);
    float val2 = smoothstep(0.003*2.0, 0.0001*2.0, distance2);
    float val3 = smoothstep(0.003*2.0, 0.0001*2.0, distance3);

    //float val1 = smoothstep(0.0F, sampval1, st.y);
    //float val2 = smoothstep(0.0F, sampval2, st.y);
    //float val3 = smoothstep(0.0F, sampval3, st.y);

    return vec4(val1, val2, val3, 0.0F);
}


out vec4 FragColor;
void main() {

    vec2 st = gl_FragCoord.xy;
    st.y /=800.0F;
    st.y -= 0.5F;
    st.y *= 2.0F;
    //st.y *= -1.0F;
    st.x /= 2400.0F;
    st.x *= 2400.0F/400.0F;


    vec4 val = vec4(0.0F, 0.0F, 0.0F, 0.0F);

    int buffer_coord = int(floor(gl_FragCoord.x));

    int sample_loc = (buffer_coord);

    if (sample_loc < 2399)
    {
        val = calc_pixel_values(sample_loc, st);
    }

    float opacity = val.x > 0.0F ? 1.0 : 0.0;
    //FragColor = vec4(val.x*vec3(0.0, 1.0, 0.9), 1.0);
    FragColor += vec4(val.y*vec3(0.0, 1.0, 0.9), 1.0);
    FragColor += vec4(val.z*vec3(1.0, 0.0, 0.0), 1.0);

    //FragColor = vec4(vec3((gl_FragCoord.x)/1200.0), 1.0);
}
