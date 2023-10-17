#uniform float iter "Iteration" 50 1 0 NULL false;
#uniform float width "Width" 1 0.1 NULL NULL false;
#uniform color c1 "Background" 0 0 0;
#uniform color c2 "Highlight" 1 1 1;

vec3 get_color(float i)
{
    if (iter - width < i && i < iter + width)
        return c2;
    else
        return c1;
}