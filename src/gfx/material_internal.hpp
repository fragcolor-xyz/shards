#pragma once

#include <vector>
#include <string>
#include "material.hpp"

namespace gfx {

enum class ShaderSourceType : uint8_t { Vertex, Pixel };

struct ShaderSource {
	std::string varyings;
	std::string source;
	ShaderSourceType type;

	ShaderSource() = default;
	bool operator==(const ShaderSource& other) const { return type == other.type && varyings == other.varyings && source == other.source; }
};

struct ShaderLibrary {
	std::string varyings;
	std::string vsSource;
	std::string psSource;

	ShaderLibrary();
	ShaderSource getShaderSource(const ShaderSourceType& type) const;
};

struct ShaderBindingPoints {
#define Binding(_name, ...) std::string _name = "u_" #_name;
#include "shaders/bindings.def"
#undef Binding
};

struct MaterialShaderCompilerInputs {
	std::vector<std::string> defines;

	static MaterialShaderCompilerInputs create(const StaticShaderParameters& params);
};

} // namespace gfx

namespace std {
template<>
struct hash<gfx::ShaderSource> {
	size_t operator()(const gfx::ShaderSource& v) const {
		size_t hash = std::hash<std::string>{}(v.source);
		hash = (hash << 1) ^ std::hash<std::string>{}(v.varyings);
		hash = (hash << 1) ^ std::hash<uint8_t>{}((uint8_t&)v.type);
		return hash;
	}
};
} // namespace std
