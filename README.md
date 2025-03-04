# 👑 Reina

Reina is a Vulkan ray tracer and the successor to [Reina OpenGL](https://www.github.com/alexanderjcs/reina-gl).

## Renders

Render of the Stanford dragon model with 24,000 samples per pixel. It has approximately 1 million triangles and took 25 minutes to render.
![image](renders/dragon5.png)

Render of the Stanford bunny model with 24,000 samples per pixel. It has approximately 70,000 triangles and took 5 minutes to render.
![image](renders/bunny2.png)

## Features

* Path tracing of the following materials:
  * Lambertian
  * Metal
  * Dielectric
* Lighting
* OBJ model loading
  * Vertex normal interpolation for smooth shading (this is how the bunny looks so smooth)
* Post-processing
  * Tone mapping


## Building

Building Reina is simple. First, clone the repository:
```shell
$ git clone
$ cd reina-vk
```

Then build the project:
```shell
$ cmake -S . -B build
$ cd build
$ cmake --build . --target reina_vk --config Release
```

The executable `reina_vk.exe` is now located in the `Release` directory.

*This project is built and tested on an RTX 3080 with Windows with the MinGW compiler. It should work on other platforms, but it has not been tested. Please [open an issue](https://www.github.com/alexanderjcs/reina-vk/issues) if you are experiencing problems.*
