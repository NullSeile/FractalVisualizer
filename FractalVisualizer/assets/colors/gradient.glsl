#uniform float colorMult "Scale" 50 1 1 NULL false;
#uniform color color1 "Start" 1 1 1;
#uniform color color2 "End" 0.3 0 1;

vec3 get_color(float i)
{
    float t = exp(-i / colorMult);
    return mix(color2, color1, t);
}