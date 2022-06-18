#uniform float colorMult 100 1 1 NULL false;
#uniform float offset 0 0.01 -1 1;
#uniform color c1 0.00 0.00 0.00;
#uniform color c2 0.60 0.22 0.26;
#uniform color c3 1.00 0.86 0.56;
#uniform color c4 0.13 0.31 0.50;

vec3 colors[] = vec3[](c1, c2, c3, c4, c1);
vec3 get_color(float iters)
{
	float x = (iters / colorMult + offset) * (colors.length() - 1);
    x = mod(x, colors.length() - 1);

    if (x == colors.length())
        x = 0.0;

    if (floor(x) == x)
        return colors[int(x)];

    int i = int(floor(x));
    float t = fract(x);
    return mix(colors[i], colors[i+1], t);
}