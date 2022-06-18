#uniform float colorMult 100 1 3 NULL false;
#uniform float offset 0 0.01 -1 1;
#uniform color c1 0.20 0.10 0.07;
#uniform color c2 0.14 0.39 0.45;
#uniform color c3 0.96 0.70 0.18;

vec3 colors[] = vec3[](c1, c2, c3, c1);
vec3 get_color(int iters)
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