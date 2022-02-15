#pragma once
#include "fwd.hpp"
#include <gfx/enums.hpp>
#include <optional>
#include <string>


namespace gfx {
namespace shader {

enum struct UniformBlock {
	View,
	Object,
};

struct FieldType {
	ShaderFieldBaseType baseType = ShaderFieldBaseType::Float32;
	size_t numComponents = 1;

	FieldType() = default;
	FieldType(ShaderFieldBaseType baseType) : baseType(baseType), numComponents(1) {}
	FieldType(ShaderFieldBaseType baseType, size_t numComponents) : baseType(baseType), numComponents(numComponents) {}
	bool operator==(const FieldType &other) const { return baseType == other.baseType && numComponents == other.numComponents; }
	bool operator!=(const FieldType &other) const { return !(*this == other); }
};

struct NamedField {
	String name;
	FieldType type;

	NamedField() = default;
	NamedField(String name, const FieldType &type) : name(name), type(type) {}
	NamedField(String name, FieldType &&type) : name(name), type(type) {}
};

struct BufferDefinition {
	String variableName;
	std::optional<String> indexedBy;
};

struct TextureDefinition {
	String variableName;
	String defaultTexcoordVariableName;
	String defaultSamplerVariableName;
};

} // namespace shader
} // namespace gfx