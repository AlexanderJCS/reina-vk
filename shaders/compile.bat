glslc -fshader-stage=rgen --target-env=vulkan1.3 raytrace.rgen.glsl -o raytrace.rgen.spv
glslc -fshader-stage=rmiss --target-env=vulkan1.3 raytrace.rmiss.glsl -o raytrace.rmiss.spv
glslc -fshader-stage=rchit --target-env=vulkan1.3 raytrace.rchit.glsl -o raytrace.rchit.spv

glslc -fshader-stage=vert --target-env=vulkan1.3 display.vert.glsl -o display.vert.spv
glslc -fshader-stage=frag --target-env=vulkan1.3 display.frag.glsl -o display.frag.spv