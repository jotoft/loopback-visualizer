#version 330
uniform int current_sample;

uniform float time;

layout(std140) uniform SamplesBlock
{
    float samples[2400];
};

/*
float plot2()
{

}
*/

float plot(vec2 st, float pct){
    return  smoothstep( pct-0.006F, pct, st.y) -
    smoothstep( pct, pct+0.006F, st.y);
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


    float val = 0.0F;

    int buffer_coord = int(floor(gl_FragCoord.x));

    int sample_loc = (buffer_coord);

    float sampval = samples[sample_loc];

    if (sample_loc < 2399)
    {
        float sampval_next = samples[sample_loc+1];
        vec2 point1 = vec2(0.0F, sampval);
        vec2 point2 = vec2(1.0F/400.0F, sampval_next);

        vec2 sample_line = point2 - point1;
        vec2 normalized = normalize(sample_line);
        vec2 rotated_normalized = vec2(normalized.y, -normalized.x);


        float distance = abs(dot(rotated_normalized, vec2(0.0F, sampval-st.y)));

        val = smoothstep(0.003*2.0, 0.0001*2.0, distance);


        //val = smoothstep(3.0F, 1.5F , st.x);
    }




    //val = plot(st, sampval);


    float opacity = val > 0.0F ? 1.0 : 0.0;
    FragColor = vec4(val*vec3(0.0, 1.0, 0.9), 1.0);
    //FragColor = vec4(vec3((gl_FragCoord.x)/1200.0), 1.0);
}
