#version 400 core

layout (location = 0) out vec4 o_Color;

layout (location = 1) out uvec4 o_Data;
layout (location = 2) out uvec2 o_Iter;

uniform usampler2D i_Data;
uniform usampler2D i_Iter;

uniform uvec2 i_Size;
uniform dvec2 i_xRange;
uniform dvec2 i_yRange;
uniform uint i_ItersPerFrame;
uniform uint i_Frame;
uniform uint i_MaxEpochs;
uniform bool i_SmoothColor;

#color

double map(double value, double inputMin, double inputMax, double outputMin, double outputMax)
{
    return outputMin + ((outputMax - outputMin) / (inputMax - inputMin)) * (value - inputMin);
}

double rand(float s) 
{
    return fract(sin(s * 12.9898) * 43758.5453);
}

dvec2 mandelbrot(dvec2 z, dvec2 c)
{
    return dvec2(z.x*z.x - z.y*z.y, 2.0 * z.x*z.y) + c;
}

void main()
{
    // Outside information
    dvec2 z;
    uint epoch;
    uint iters;
    if (i_Frame == 0)
    {
        z = dvec2(0, 0);
        epoch = 0;
        iters = 0;
    }
    else
    {
        uvec4 data = texture(i_Data, gl_FragCoord.xy / i_Size);
        z.x = packDouble2x32(data.xy);
        z.y = packDouble2x32(data.zw);

        uvec2 iter_data = texture(i_Iter, gl_FragCoord.xy / i_Size).xy;
        epoch = iter_data.x;
        iters = iter_data.y;
    }
    
    // Stop at max epochs
    if (epoch >= i_MaxEpochs && i_MaxEpochs > 0)
    {
        o_Data = uvec4(unpackDouble2x32(z.x), unpackDouble2x32(z.y));
        o_Iter = uvec2(epoch, iters);
        o_Color = vec4(0.0, 0.0, 0.0, 0.0);
        return;
    }

    // Set c
    dvec2 c;
    dvec2 pos = gl_FragCoord.xy + dvec2(rand(epoch), rand(epoch + 1));
    c.x = map(pos.x, 0, i_Size.x, i_xRange.x, i_xRange.y);
    c.y = map(pos.y, 0, i_Size.y, i_yRange.x, i_yRange.y);

    // Calculate the iterations
    int i;
    for (i = 0; i < i_ItersPerFrame && z.x*z.x + z.y*z.y <= 100.0; i++)
    {
        z = mandelbrot(z, c);
    }

    // Output the data
    if (i == i_ItersPerFrame)
    {
        o_Data = uvec4(unpackDouble2x32(z.x), unpackDouble2x32(z.y));
        o_Iter = uvec2(epoch, iters + i);
        o_Color = vec4(0.0, 0.0, 0.0, 1e-10);
    }
    else
    {
        o_Data = uvec4(unpackDouble2x32(0), unpackDouble2x32(0));
        o_Iter = uvec2(epoch + 1, 0);

        vec3 color;
        int n = int(iters) + i;
        if (i_SmoothColor)
        {
            float log_zn = log(float(z.x*z.x + z.y*z.y)) / 2.0;
            float nu = log(log_zn / log(2.0)) / log(2.0);

            float new_iter = n + 1 - nu + 1.3;

            color = get_color(new_iter);
        }
        else
            color = get_color(n);

        o_Color = vec4(color, 1.0 / float(epoch + 1));
    }
}