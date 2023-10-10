# Fractal visualizer
WIP fractal visualizer build on top of https://github.com/TheCherno/OpenGL.

## Showcase

![screenshot01](https://github.com/NullSeile/FractalVisualizer/assets/18171867/c2aef8cf-f262-4f50-a41b-d27d8e4f17c0)

https://github.com/NullSeile/FractalVisualizer/assets/18171867/1d29cdc4-1ddd-428f-8677-437b5257a015

https://github.com/NullSeile/FractalVisualizer/assets/18171867/712bb0b8-31e3-4ade-98a7-d899ac054eb9

https://github.com/NullSeile/FractalVisualizer/assets/18171867/a635ad88-aacd-48b5-a676-33297f6f6acd


## Main functionality

- You can pan and zoom by dragging and using the mouse wheel.
- Left click the mandelbrot set to set the julia c to the mouse location.
- CTRL + left click to set the center to the mouse location.
- Middle mouse button to show the first iterations of the equation.
- Hold CTRL while releasing the middle mouse button to persistently show the iterations.

## Color function shaders

You can choose how to colorize the image by changing the color function. The program comes with a few examples.

If you wish to create your own custom color, function all you have to do is create a .glsl file in the `./assets/colors` folder. The file should implement the following function:

```glsl
vec3 get_color(float i) {
    ...
}
```
This function takes the number of iterations a point lasted before diverging and returns the color of that point.

You can also add uniforms which will be visible and editable in the UI. To do this you add the preprocessor statement `#uniform`. There are the following types of uniforms:

- **Float:** `#uniform float <name> <display_name> <default_value> <slider_increment> <min> <max>;`. Either min or max can be set to `NULL` to indicate it is unbounded.
- **Boolean:** `#uniform bool <name> <display_name> <default_value>;`. The default value must be either `true` or `false`.
- **Color:** `#uniform color <name> <display_name> <default_red> <default_green> <default_blue>;`. The RGB values must be between 0 and 1.

Additionaly, all types accept an optional boolean parameter at the end (defaults to `true`) which indicates whether this parameter should update the preview image.

Note that, unlike vanilla glsl preprocessor statements, the uniform preprocessor must end with a semicolon.

## Build

Currently only "officially" supports Windows.

```
git clone --recursive https://github.com/NullSeile/FractalVisualizer.git
```

Run `scripts/Win-Premake.bat` and open `FractalVisualizer.sln` in Visual Studio 2022. `FractalVisualizer/src/MainLayer.cpp` contains the OpenGL code that's running.
