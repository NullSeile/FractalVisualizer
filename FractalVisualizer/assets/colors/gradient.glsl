#uniform float colorMult 50 1 1 NULL false;
#uniform color color1 0.1 0.1 1;
#uniform color color2 1 1 1;

vec3 get_color(int i)
{
    float t = exp(-i / colorMult);
    return mix(color2, color1, t);
}