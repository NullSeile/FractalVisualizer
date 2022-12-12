#uniform float colorMult 100 1 1 NULL false;
#uniform float offset 0 0.01 -1 1;
#uniform color c1 0.23 0.18 0.12;
#uniform color c2 0.56 0.48 0.28;
#uniform color c3 0.83 0.75 0.53;
#uniform color c4 0.90 0.87 0.73;
#uniform color c5 0.29 0.42 0.24;
#uniform color c6 0.23 0.29 0.16;
#uniform bool sine_interp true;

#define PI 3.1415926535
vec3 colors[] = vec3[](c1, c2, c3, c4, c5, c6, c1);
vec3 get_color(float iters)
{
	float x = (iters / colorMult + offset) * (colors.length() - 1);
    x = mod(x, colors.length() - 1);

    if (x == colors.length())
        x = 0.0;

    int i = int(floor(x));
    float t = fract(x);

    if (sine_interp)
        t = -cos(PI * t) * 0.5 + 0.5;

    return mix(colors[i], colors[i+1], t);
}