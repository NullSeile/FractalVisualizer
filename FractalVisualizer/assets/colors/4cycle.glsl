#uniform float colorMult "Scale" 100 1 1 NULL false;
#uniform float offset "Offset" 0 0.01 -1 1;
#uniform color c1 "1" 0.00 0.00 0.00;
#uniform color c2 "2" 0.60 0.22 0.26;
#uniform color c3 "3" 1.00 0.86 0.56;
#uniform color c4 "4" 0.13 0.31 0.50;
#uniform bool sine_interp "Use sine interpolation" true;

#define PI 3.1415926535
vec3 colors[] = vec3[](c1, c2, c3, c4, c1);
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