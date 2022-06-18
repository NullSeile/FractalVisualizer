#uniform float colorMult 50 1 1 NULL false;
#uniform color color1 1 1 1;
#uniform color color2 0.3 0 1;

vec3 get_color(int i)
{
    float t = exp(-i / colorMult);
    return mix(color2, color1, t);
}