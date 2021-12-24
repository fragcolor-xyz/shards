#pragma once
#include "linalg.hpp"
#include <stdint.h>
#include <memory>
#include <variant>

namespace gfx {

struct Texture;
typedef std::shared_ptr<Texture> TexturePtr;

enum class FieldType : uint8_t {
#define FIELD_TYPE(_0, _1, _name, ...) _name,
#include "field_types.def"
};

typedef std::variant<float, float2, float3, float4, float4x4, TexturePtr> FieldVariant;

void packFieldVariant(const FieldVariant &variant, std::vector<uint8_t> &outBuffer);
FieldType getFieldVariantType(const FieldVariant &variant);

// True if type is anything except texture
bool isBasicFieldType(const FieldType& type);
const char *getBasicFieldGLSLName(FieldType type);
bool getBasicFieldGLSLValue(FieldType type, const FieldVariant &variant, std::string& out);
void getBasicFieldGLSLDefaultValue(FieldType type, std::string &out);
inline void getBasicFieldGLSLValueOrDefault(FieldType type, const FieldVariant &variant, std::string &out) {
	if (!getBasicFieldGLSLValue(type, variant, out))
		getBasicFieldGLSLDefaultValue(type, out);
}

struct IDrawDataCollector {
#define FIELD_TYPE(_name, _cppType, ...) virtual void set_##_name(const char* name, const _cppType &_##_name) = 0;
#include "field_types.def"
};

} // namespace gfx
