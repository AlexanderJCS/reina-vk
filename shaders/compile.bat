glslc -fshader-stage=rgen --target-env=vulkan1.3 raytrace.rgen.glsl -o raytrace.rgen.spv
glslc -fshader-stage=rmiss --target-env=vulkan1.3 raytrace.rmiss.glsl -o raytrace.rmiss.spv
glslc -fshader-stage=rchit --target-env=vulkan1.3 raytrace.rchit.glsl -o raytrace.rchit.spv