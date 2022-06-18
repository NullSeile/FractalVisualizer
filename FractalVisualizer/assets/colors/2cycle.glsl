#uniform float colorMult 100 1 1 NULL false;
#uniform float offset 0 0.01 -1 1;
#uniform color c1 0.13 0.08 0.20;
#uniform color c2 0.92 0.83 0.49;
#uniform bool sine_interpolation false;

#define PI 3.1415926535
vec3 get_color(float iters)
{
    float x = iters / colorMult + offset;

    if (sine_interpolation)
    {
        float t = -cos(2 * PI * x) * 0.5 + 0.5;
        return mix(c1, c2, t);
    }
    else
    {
        float t = fract(x) * 2.0;
        if (t > 1)
            t = 2 - t;
        
        return mix(c1, c2, t);
    }
}