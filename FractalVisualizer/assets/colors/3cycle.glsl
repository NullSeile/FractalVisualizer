#uniform float colorMult "Scale" 100 1 1 NULL false;
#uniform float offset "Offset" 0 0.01 -1 1;
#uniform color c1 "1" 0.07 0.14 0.25;
#uniform color c2 "2" 0.72 0.91 0.83;
#uniform color c3 "3" 0.92 0.58 0.44;
#uniform bool sine_interp "Use sine interpolation" true;

#define PI 3.1415926535
vec3 colors[] = vec3[](c1, c2, c3, c1);
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