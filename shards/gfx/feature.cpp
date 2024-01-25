
#include "feature.hpp"

namespace gfx {
using shader::NumType;

UniqueIdGenerator featureIdGenerator(UniqueIdTag::Feature);

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

NumParamDecl::NumParamDecl(const NumType &type, NumParameter defaultValue)
    : type(type), defaultValue(defaultValue) {}
NumParamDecl::NumParamDecl(NumParameter defaultValue)
    : type(getNumParameterType(defaultValue)), defaultValue(defaultValue) {}

} // namespace gfx
