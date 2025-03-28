glslc -O -I "../polyglot" -fshader-stage=rgen --target-env=vulkan1.3 ./raytrace/raytrace.rgen.glsl -o ./raytrace/raytrace.rgen.spv
glslc -O -I "../polyglot" -fshader-stage=rmiss --target-env=vulkan1.3 ./raytrace/raytrace.rmiss.glsl -o ./raytrace/raytrace.rmiss.spv
glslc -O -I "../polyglot" -fshader-stage=rchit --target-env=vulkan1.3 ./raytrace/lambertian.rchit.glsl -o ./raytrace/lambertian.rchit.spv
glslc -O -I "../polyglot" -fshader-stage=rchit --target-env=vulkan1.3 ./raytrace/metal.rchit.glsl -o ./raytrace/metal.rchit.spv
glslc -O -I "../polyglot" -fshader-stage=rchit --target-env=vulkan1.3 ./raytrace/dielectric.rchit.glsl -o ./raytrace/dielectric.rchit.spv
glslc -O -I "../polyglot" -fshader-stage=rmiss --target-env=vulkan1.3 ./raytrace/shadow.rmiss.glsl -o ./raytrace/shadow.rmiss.spv

glslc -O -I "../polyglot" -fshader-stage=vert --target-env=vulkan1.3 ./raster/display.vert.glsl -o ./raster/display.vert.spv
glslc -O -I "../polyglot" -fshader-stage=frag --target-env=vulkan1.3 ./raster/display.frag.glsl -o ./raster/display.frag.spv

glslc -O -I "../polyglot" -fshader-stage=comp --target-env=vulkan1.3 ./postprocessing/tonemap/postprocessing.comp.glsl -o ./postprocessing/tonemap/postprocessing.comp.spv

glslc -O -I "../polyglot" -fshader-stage=comp --target-env=vulkan1.3 ./postprocessing/bloom/blurX.comp.glsl -o ./postprocessing/bloom/blurX.comp.spv
glslc -O -I "../polyglot" -fshader-stage=comp --target-env=vulkan1.3 ./postprocessing/bloom/blurY.comp.glsl -o ./postprocessing/bloom/blurY.comp.spv
glslc -O -I "../polyglot" -fshader-stage=comp --target-env=vulkan1.3 ./postprocessing/bloom/combine.comp.glsl -o ./postprocessing/bloom/combine.comp.spv