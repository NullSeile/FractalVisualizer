#uniform float colorMult 100 1 3 NULL false;
#uniform float offset 0 0.01 -1 1;
#uniform color c1 0.00 0.00 0.00;
#uniform color c2 1.00 1.00 1.00;

vec3 get_color(int iters)
{
    float x = fract(iters / colorMult + offset) * 2;

    float t = x;
    if (x > 1)
        t = 2 - x;
    
    vec3 color = mix(c1, c2, t);

    return color;
}