
#include "feature.hpp"

namespace gfx {

NamedShaderParam::NamedShaderParam(std::string name, ShaderParamType type, ParamVariant defaultValue) : type(type), name(name), defaultValue(defaultValue) {}
NamedShaderParam::NamedShaderParam(std::string name, ParamVariant defaultValue)
	: type(getParamVariantType(defaultValue)), name(name), defaultValue(defaultValue) {}


} // namespace gfx