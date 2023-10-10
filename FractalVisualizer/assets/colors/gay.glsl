#uniform float colorMult "Scale" 100 1 1 NULL false;
#uniform float saturation "Saturation" 1 0.01 0 1;
#uniform float brightness "Brightness" 1 0.01 0 1;
#uniform float offset "Offset" 0 0.01 -1 1;
vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

vec3 get_color(float i)
{
    return hsv2rgb(vec3(i / colorMult + offset, saturation, brightness));
}
