# Fractal visualizer
WIP fractal visualizer build on top of https://github.com/TheCherno/OpenGL.

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

- **Float:** `#uniform float <name> <default_value> <slider_increment> <min> <max>;`. Either min or max can be set to `NULL` to indicate it is unbounded.
- **Boolean:** `#uniform bool <name> <default_value>;`. The default value must be either `true` or `false`.
- **Color:** `#uniform color <name> <default_red> <default_green> <default_blue>;`. The RGB values must be between 0 and 1.

Additionaly, all types accept an optional boolean parameter at the end (defaults to `true`) which indicates whether this parameter should update the preview image.

Note that, unlike vanilla glsl preprocessor statements, the uniform preprocessor must end with a semicolon.

## Build

Currently only "officially" supports Windows.

```
git clone --recursive https://github.com/NullSeile/FractalVisualizer.git
```

Run `scripts/Win-Premake.bat` and open `FractalVisualizer.sln` in Visual Studio 2022. `FractalVisualizer/src/MainLayer.cpp` contains the OpenGL code that's running.
