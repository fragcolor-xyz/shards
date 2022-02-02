#pragma once
#include "linalg.hpp"
#include <memory>
#include <stdint.h>
#include <string>
#include <unordered_map>
#include <variant>

namespace gfx {

struct Texture;
typedef std::shared_ptr<Texture> TexturePtr;

enum class ShaderParamType : uint8_t {
#define FIELD_TYPE(_cppType, _displayName, ...) _displayName,
#include "field_types.def"
#undef FIELD_TYPE
};

typedef std::variant<float, float2, float3, float4, float4x4> FieldVariant;

size_t packFieldVariant(uint8_t* outData, size_t outLength, const FieldVariant &variant);
ShaderParamType getFieldVariantType(const FieldVariant &variant);
size_t getFieldTypeSize(ShaderParamType type);
size_t getFieldTypeWGSLAlignment(ShaderParamType type);

struct IDrawDataCollector {
#define FIELD_TYPE(_cppType, _displayName, ...) virtual void set_##_cppType(const char *name, const _cppType &_##_name) = 0;
#include "field_types.def"
#undef FIELD_TYPE
};

struct DrawData : IDrawDataCollector {
	std::unordered_map<std::string, FieldVariant> data;

#define FIELD_TYPE(_cppType, _displayName, ...) \
	void set_##_cppType(const char *name, const _cppType &v) { data[name] = v; }
#include "field_types.def"
#undef FIELD_TYPE
	void append(const DrawData &other) {
		for (auto &it : other.data) {
			data.insert_or_assign(it.first, it.second);
		}
	}
};

} // namespace gfx
