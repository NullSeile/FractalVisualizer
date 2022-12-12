#uniform float colorMult 100 1 1 NULL false;
#uniform float offset 0 0.01 -1 1;
#uniform color c1 0.00 0.08 0.15;
#uniform color c2 0.26 0.48 0.63;
#uniform color c3 0.99 1.00 0.85;
#uniform color c4 0.58 0.21 0.37;
#uniform color c5 0.32 0.00 0.17;
#uniform bool sine_interp true;

#define PI 3.1415926535
vec3 colors[] = vec3[](c1, c2, c3, c4, c5, c1);
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