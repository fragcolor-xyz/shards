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
#define PARAM_TYPE(_cppType, _displayName, ...) _displayName,
#include "param_types.def"
#undef PARAM_TYPE
};

typedef std::variant<float, float2, float3, float4, float4x4> ParamVariant;

size_t packParamVariant(uint8_t* outData, size_t outLength, const ParamVariant &variant);
ShaderParamType getParamVariantType(const ParamVariant &variant);
size_t getParamTypeSize(ShaderParamType type);
size_t getParamTypeWGSLAlignment(ShaderParamType type);

struct IDrawDataCollector {
#define PARAM_TYPE(_cppType, _displayName, ...) virtual void set_##_cppType(const char *name, const _cppType &_##_name) = 0;
#include "param_types.def"
#undef PARAM_TYPE
};

struct DrawData : IDrawDataCollector {
	std::unordered_map<std::string, ParamVariant> data;

#define PARAM_TYPE(_cppType, _displayName, ...) \
	void set_##_cppType(const char *name, const _cppType &v) { data[name] = v; }
#include "param_types.def"
#undef PARAM_TYPE
	void append(const DrawData &other) {
		for (auto &it : other.data) {
			data.insert_or_assign(it.first, it.second);
		}
	}
};

} // namespace gfx
