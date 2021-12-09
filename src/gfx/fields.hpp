#pragma once
#include "linalg.hpp"
#include <cstdint>
#include <memory>
#include <variant>

namespace gfx {

struct Texture;
typedef std::shared_ptr<Texture> TexturePtr;

enum class FieldType : uint8_t {
#define FIELD_TYPE(_0, _1, _name, ...) _name,
#include "field_types.def"
};

typedef std::variant<float2, float3, float4, float4x4, TexturePtr> FieldVariant;

void packFieldVariant(const FieldVariant& variant, std::vector<uint8_t>& outBuffer);

struct IDrawDataCollector {
#define FIELD_TYPE(_name, _cppType, ...) void set_##_name(const _cppType &_name);
#include "field_types.def"
};

} // namespace gfx
