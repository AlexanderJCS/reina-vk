glslc -fshader-stage=rgen --target-env=vulkan1.3 raytrace.rgen.glsl -o raytrace.rgen.spv
glslc -fshader-stage=rmiss --target-env=vulkan1.3 raytrace.rmiss.glsl -o raytrace.rmiss.spv
glslc -fshader-stage=rchit --target-env=vulkan1.3 lambertian.rchit.glsl -o lambertian.rchit.spv
glslc -fshader-stage=rchit --target-env=vulkan1.3 metal.rchit.glsl -o metal.rchit.spv
glslc -fshader-stage=rchit --target-env=vulkan1.3 dielectric.rchit.glsl -o dielectric.rchit.spv
glslc -fshader-stage=rmiss --target-env=vulkan1.3 shadow.rmiss.glsl -o shadow.rmiss.spv

glslc -fshader-stage=vert --target-env=vulkan1.3 display.vert.glsl -o display.vert.spv
glslc -fshader-stage=frag --target-env=vulkan1.3 display.frag.glsl -o display.frag.spv

glslc -fshader-stage=comp --target-env=vulkan1.3 postprocessing.comp.glsl -o postprocessing.comp.spv