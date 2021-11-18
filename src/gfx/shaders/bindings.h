#include <bgfx_shader.h>

#define Binding_Texture2D(_x) SAMPLER2D(_x)
#define Binding_Float(_x) uniform float _x
#define Binding_Float3(_x) uniform vec3 _x
#define Binding_Float4(_x) uniform vec4 _x
#define Binding(_name, _bindingFunc) _bindingFunc(_name)
#include "bindings.def"
