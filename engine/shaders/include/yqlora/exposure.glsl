// Экспозиция вида: всё, что пишется в HDR-цель, проходит через exposed().
#ifndef YQLORA_EXPOSURE_GLSL
#define YQLORA_EXPOSURE_GLSL

#include <yqlora/uniforms.glsl>

// Чуть ниже предела половинного float (65504): диск солнца при тёмной
// экспозиции не превращается в бесконечность.
const float YQLORA_MAX_EXPOSED = 60000.0;

// Яркость в нитах с экспозицией вида, готовая для HDR-цели.
vec3 exposed(vec3 luminance)
{
    return min(luminance * view.exposure, vec3(YQLORA_MAX_EXPOSED));
}

#endif
