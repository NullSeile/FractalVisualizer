#uniform float colorMult 100 1 1 NULL false;
#uniform float offset 0 0.02 -1 1;
#define PI 3.1415926535
vec3 get_color(float i)
{
    float x = i / colorMult + offset;
    float n1 = sin(2*PI*x) * 0.5 + 0.5;
    float n2 = cos(2*PI*x) * 0.5 + 0.5;
    return vec3(n1, n2, 1.0);
}