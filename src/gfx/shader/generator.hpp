#pragma once

#include "block.hpp"
#include "entry_point.hpp"
#include "textures.hpp"
#include "types.hpp"
#include "uniforms.hpp"
#include <gfx/mesh.hpp>
#include <gfx/params.hpp>
#include <map>
#include <optional>
#include <string>

namespace gfx {
namespace shader {
using String = std::string;
using StringView = std::string_view;

struct GeneratorError {
	std::string error;
};

struct GeneratorContext {
	String result;
	String inputVariableName;
	String outputVariableName;
	String globalsVariableName;
	std::map<String, BufferDefinition> buffers;
	std::map<String, TextureDefinition> textures;
	std::map<String, const NamedField *> inputs;
	std::map<String, FieldType> globals;
	std::map<String, FieldType> outputs;
	std::map<String, size_t> sampleTextures;
	bool canAddOutputs = false;
	std::vector<GeneratorError> errors;

	void write(const StringView &str);

	void readGlobal(const char *name);
	void writeGlobal(const char *name, const FieldType &type);

	bool hasInput(const char *name);
	void readInput(const char *name);

	bool hasOutput(const char *name);
	void writeOutput(const char *name, const FieldType &type);

	const TextureDefinition* getTexture(const char *name);
	void texture(const char *name);
	void textureDefaultTextureCoordinate(const char *name);
	void textureDefaultSampler(const char *name);

	void readBuffer(const char *name);

	void pushError(GeneratorError &&error);
};

struct GeneratorOutput {
	String wgslSource;
	std::vector<GeneratorError> errors;

	static void dumpErrors(const GeneratorOutput &output);
};

struct Generator {
	UniformBufferLayout objectBufferLayout;
	TextureBindingLayout textureBindingLayout;
	UniformBufferLayout viewBufferLayout;
	MeshFormat meshFormat;
	std::vector<NamedField> outputFields;

	GeneratorOutput build(const std::vector<EntryPoint> &entryPoints);
	GeneratorOutput build(const std::vector<const EntryPoint *> &entryPoints);
};
} // namespace shader
} // namespace gfx
