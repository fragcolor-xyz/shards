
#include "feature.hpp"

namespace gfx {

BlendComponent BlendComponent::Opaque = BlendComponent{
    .operation = WGPUBlendOperation_Max,
    .srcFactor = WGPUBlendFactor::WGPUBlendFactor_One,
    .dstFactor = WGPUBlendFactor::WGPUBlendFactor_One,
};
BlendComponent BlendComponent::Additive = BlendComponent{
    .operation = WGPUBlendOperation_Add,
    .srcFactor = WGPUBlendFactor::WGPUBlendFactor_One,
    .dstFactor = WGPUBlendFactor::WGPUBlendFactor_One,
};
BlendComponent BlendComponent::Alpha = BlendComponent{
    .operation = WGPUBlendOperation_Add,
    .srcFactor = WGPUBlendFactor::WGPUBlendFactor_SrcAlpha,
    .dstFactor = WGPUBlendFactor::WGPUBlendFactor_OneMinusSrcAlpha,
};
BlendComponent BlendComponent::AlphaPremultiplied = BlendComponent{
    .operation = WGPUBlendOperation_Add,
    .srcFactor = WGPUBlendFactor::WGPUBlendFactor_One,
    .dstFactor = WGPUBlendFactor::WGPUBlendFactor_OneMinusSrcAlpha,
};

NamedShaderParam::NamedShaderParam(std::string name, ShaderParamType type, ParamVariant defaultValue)
    : type(type), name(name), defaultValue(defaultValue) {}
NamedShaderParam::NamedShaderParam(std::string name, ParamVariant defaultValue)
    : type(getParamVariantType(defaultValue)), name(name), defaultValue(defaultValue) {}

} // namespace gfx