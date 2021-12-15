#include "material_shader.hpp"
#include "bgfx/bgfx.h"
#include "bgfx/defines.h"
#include "context.hpp"
#include "hash.hpp"
#include "hasherxxh128.hpp"
#include "material.hpp"
#include "shaderc.hpp"
#include "spdlog/spdlog.h"
#include "texture.hpp"
#include "types.hpp"
#include <boost/algorithm/string/case_conv.hpp>
#include <boost/algorithm/string/join.hpp>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace gfx {

MaterialBuilderContext::MaterialBuilderContext(Context &context) : context(context) {
	fallbackTexture = createFallbackTexture();
	fallbackShaderProgram = createFallbackShaderProgram();
	defaultVectorParameters = std::unordered_map<std::string, float4>{
		{"baseColor", float4(1.0f, 1.0f, 1.0f, 1.0f)},
		// {"roughness", float4(1.0f)},
		// {"metallicness", float4(0.0f)},
	};
}

MaterialBuilderContext::~MaterialBuilderContext() {
	for (auto &uniform : uniformsCache) {
		bgfx::destroy(uniform.second);
	}
	uniformsCache.clear();
}

ShaderHandlePtr MaterialBuilderContext::getCachedShader(Hash128 inputHash) {
	auto it = compiledShaders.find(inputHash);
	if (it != compiledShaders.end())
		return it->second;
	return ShaderHandlePtr();
}

ShaderProgramPtr MaterialBuilderContext::getCachedProgram(Hash128 inputHash) {
	auto it = compiledPrograms.find(inputHash);
	if (it != compiledPrograms.end())
		return it->second;
	return ShaderProgramPtr();
}

bgfx::UniformHandle MaterialBuilderContext::getUniformHandle(const std::string &name, bgfx::UniformType::Enum type, uint32_t num) {
	auto it = uniformsCache.find(name);
	if (it == uniformsCache.end()) {
		std::string uniformName = std::string("u_") + name;
		bgfx::UniformHandle handle = bgfx::createUniform(uniformName.c_str(), type, num);

		it = uniformsCache.insert(std::make_pair(name, handle)).first;
	}
	return it->second;
}

TexturePtr MaterialBuilderContext::createFallbackTexture() {
	Colorf fallbackTextureColor = Colorf(1.0f, 0.0f, 1.0f, 1.0f);
	uint32_t fallbackTextureColors[4];
	for (size_t i = 0; i < sizeof(fallbackTextureColors) / sizeof(uint32_t); i++) {
		fallbackTextureColors[i] = colorToARGB(colorFromFloat(fallbackTextureColor));
	}

	TexturePtr texture = std::make_shared<Texture>(int2(2, 2), bgfx::TextureFormat::RGBA8);
	texture->update(bgfx::copy(fallbackTextureColors, sizeof(fallbackTextureColors)));
	return texture;
}

ShaderProgramPtr MaterialBuilderContext::createFallbackShaderProgram() {
	ShaderProgramPtr shaderProgram;
	return shaderProgram;
}

ShaderProgramPtr MaterialUsageContext::getProgram() {
	// Hash the external parameters
	HasherXXH128<HashStaticVistor> hasher;
	hasher(*this);
	Hash128 materialContextHash = hasher.getDigest();

	ShaderProgramPtr program = context.getCachedProgram(materialContextHash);
	if (program)
		return program;

	program = compileProgram();
	context.compiledPrograms[materialContextHash] = program;

	if (!program) {
		program = context.fallbackShaderProgram;
	}

	return program;
}

struct MaterialShaderBuilder {
	std::vector<std::string> attributeNames;
	std::vector<std::string> varyingNames;
	std::vector<std::string> defines;
	std::stringstream varyings;
	std::stringstream vsCode, psCode;
	std::unordered_map<std::string, size_t> textureRegisterMap;
	bool usesMultipleRenderTargets = false;

	void addGenericAttribute(std::string name, const char *type) {
		std::string nameUpper = name;
		boost::algorithm::to_upper(nameUpper);

		attributeNames.push_back("a_" + name);
		varyings << type << " a_" << name << " : " << nameUpper << ";\n";
		defines.push_back("GFX_HAS_VERTEX_" + nameUpper + "=1");
	};

	void addVarying(std::string name, const char *type) {
		std::string nameUpper = name;
		boost::algorithm::to_upper(nameUpper);

		varyingNames.push_back("v_" + name);
		varyings << type << " v_" << name << " : " << nameUpper << ";\n";
	};

	void assignTextureSlots(const MaterialData &materialData, const std::unordered_map<std::string, MaterialTextureSlot> &usageTextureSlots) {
		size_t textureCounter = 0;
		std::string textureFieldsDefine = "";

		auto process = [&](const std::unordered_map<std::string, MaterialTextureSlot> &textureSlots) {
			for (auto &textureSlot : textureSlots) {
				if (textureSlot.first == MaterialBuiltin::baseColorTexture) {
					defines.push_back(fmt::format("GFX_BASE_COLOR_TEXTURE={}", textureSlot.second.texCoordIndex));
				} else if (textureSlot.first == MaterialBuiltin::normalTexture) {
					defines.push_back(fmt::format("GFX_NORMAL_TEXTURE={}", textureSlot.second.texCoordIndex));
				} else if (textureSlot.first == MaterialBuiltin::metalicRoughnessTexture) {
					defines.push_back(fmt::format("GFX_METALLIC_ROUGHNESS_TEXTURE={}", textureSlot.second.texCoordIndex));
				} else if (textureSlot.first == MaterialBuiltin::emissiveTexture) {
					defines.push_back(fmt::format("GFX_EMISSIVE_TEXTURE={}", textureSlot.second.texCoordIndex));
				}

				TexturePtr texture = textureSlot.second.texture;
				TextureType type = texture ? texture->getType() : TextureType::Texture2D;
				if (type == TextureType::Texture2D) {
					textureFieldsDefine += (fmt::format("SAMPLER2D(u_{0}, u_{0}_register);", textureSlot.first));
				} else if (type == TextureType::TextureCube) {
					textureFieldsDefine += (fmt::format("SAMPLERCUBE(u_{0}, u_{0}_register);", textureSlot.first));
				} else {
					assert(false);
				}

				defines.push_back(fmt::format("u_{}_register={}", textureSlot.first, textureCounter));
				defines.push_back(fmt::format("u_{}_texcoord=v_texcoord{}", textureSlot.first, textureSlot.second.texCoordIndex));
				textureRegisterMap[textureSlot.first] = textureCounter;
				textureCounter++;
			}
		};
		process(materialData.textureSlots);
		process(usageTextureSlots);

		defines.push_back("TEXTURE_FIELDS=" + textureFieldsDefine);
	}

	void setupLighting(const MaterialData &materialData, const StaticMaterialOptions &staticOptions) {
		defines.push_back(fmt::format("GFX_MAX_DIR_LIGHTS={}", staticOptions.numDirectionLights));
		defines.push_back(fmt::format("GFX_MAX_POINT_LIGHTS={}", staticOptions.numPointLights));
		if (staticOptions.hasEnvironmentLight) {
			defines.push_back(fmt::format("GFX_ENVIRONMENT_LIGHT={}", 1));
		}
	}

	void build(const MaterialData &materialData, const StaticMaterialOptions &staticOptions,
			   const std::unordered_map<std::string, MaterialTextureSlot> &usageTextureSlots) {
		addGenericAttribute("position", "vec3");
		addGenericAttribute("texcoord0", "vec2");

		if (staticOptions.usageFlags & MaterialUsageFlags::HasNormals) {
			addGenericAttribute("normal", "vec3");
		}

		if (staticOptions.usageFlags & MaterialUsageFlags::HasTangents) {
			addGenericAttribute("tangent", "vec3");
		}

		if (staticOptions.usageFlags & MaterialUsageFlags::HasVertexColors) {
			addGenericAttribute("color0", "vec4");
		}

		if (staticOptions.usageFlags & MaterialUsageFlags::FlipTexcoordY) {
			defines.push_back("GFX_FLIP_TEX_Y=1");
		}

		addVarying("texcoord0", "vec2");
		addVarying("normal", "vec3");
		addVarying("tangent", "vec3");
		addVarying("color0", "vec4");
		addVarying("worldPosition", "vec3");

		assignTextureSlots(materialData, usageTextureSlots);

		if (materialData.flags & MaterialStaticFlags::Lit) {
			setupLighting(materialData, staticOptions);
			defines.push_back("GFX_LIT=1");
		}

		if (materialData.flags & MaterialStaticFlags::Fullscreen) {
			defines.push_back("GFX_FULLSCREEN=1");
		}

		if (materialData.pixelCode.size() > 0) {
			if (materialData.mrtOutputs.size() > 0) {
				usesMultipleRenderTargets = true;
				std::string mrtOutputFields;
				std::string mrtOutputAssignments;
				for (uint32_t mrtOutputIndex : materialData.mrtOutputs) {
					mrtOutputFields += fmt::format("vec4 color{};", mrtOutputIndex);
					mrtOutputAssignments += fmt::format("gl_FragData[{0}] = mi.color{0};", mrtOutputIndex);
				}
				defines.push_back(fmt::format("GFX_MRT_FIELDS={}", mrtOutputFields));
				defines.push_back(fmt::format("GFX_MRT_ASSIGNMENTS={}", mrtOutputAssignments));
				defines.push_back(fmt::format("DEFAULT_COLOR_FIELD=color{}", materialData.mrtOutputs[0]));
			}
		}

		generateVertexCode(materialData);
		generatePixelCode(materialData);
	}

	void generateVertexCode(const MaterialData &materialData) {
		vsCode << "$input " << boost::algorithm::join(attributeNames, ", ") << "\n";
		vsCode << "$output " << boost::algorithm::join(varyingNames, ", ") << "\n";
		vsCode << "#include <vs_entry.sh>\n";

		if (materialData.vertexCode.size() > 0) {
			defines.push_back("GFX_HAS_VS_MATERIAL_MAIN=1");
			vsCode << "#define main materialMain\n";
			vsCode << materialData.vertexCode;
		}
	}

	void generatePixelCode(const MaterialData &materialData) {
		psCode << "$input " << boost::algorithm::join(varyingNames, ", ") << "\n";
		psCode << "#include <ps_entry.sh>\n";

		if (materialData.pixelCode.size() > 0) {

			defines.push_back("GFX_HAS_PS_MATERIAL_MAIN=1");
			psCode << "#define main materialMain\n";
			psCode << materialData.pixelCode;
		}
	}
};

ShaderProgramPtr MaterialUsageContext::compileProgram() {
	MaterialShaderBuilder shaderBuilder;
	shaderBuilder.build(material.getData(), staticOptions, textureSlots);

	std::string varyings = shaderBuilder.varyings.str();
	std::string vsCode = shaderBuilder.vsCode.str();
	std::string psCode = shaderBuilder.psCode.str();

	HasherXXH128 hasher;
	hasher("vertex");
	hasher(varyings);
	hasher(vsCode);
	hasher(shaderBuilder.defines);
	Hash128 vsCodeHash = hasher.getDigest();

	hasher.reset();
	hasher("pixel");
	hasher(varyings);
	hasher(psCode);
	hasher(shaderBuilder.defines);

	Hash128 psCodeHash = hasher.getDigest();

	spdlog::info("Shader varyings:\n{}", varyings);

	auto compile = [&](char type, ShaderCompileOutput &output, std::string code) {
		IShaderCompiler &shaderCompiler = context.context.shaderCompilerModule->getShaderCompiler();
		ShaderCompileOptions options;
		options.shaderType = type;
		options.defines = shaderBuilder.defines;
		options.setCompatibleForContext(context.context);
		options.optimize = false;

		options.disasm = true;
		options.debugInformation = true;
		options.verbose = true;
		options.keepIntermediate = true;
		options.keepOutputs = true;

		spdlog::info("Shader source:\n{}", code);
		return shaderCompiler.compile(options, output, varyings.c_str(), code.data(), code.length());
	};

	// Required for OpenGL to find uniforms, the uniforms need to be defined before loading the shader program
	preloadUniforms();

	ShaderHandlePtr vertexShader = context.getCachedShader(vsCodeHash);
	if (!vertexShader) {
		ShaderCompileOutput output;
		bool vsCompiled = compile('v', output, vsCode);
		if (!vsCompiled) {
			spdlog::error("Failed to compiler vertex shader");
			return ShaderProgramPtr();
		}
		bgfx::ShaderHandle shaderHandle = bgfx::createShader(bgfx::copy(output.binary.data(), output.binary.size()));
		vertexShader = std::make_shared<ShaderHandle>(shaderHandle);

		context.compiledShaders[vsCodeHash] = vertexShader;
	}

	ShaderHandlePtr pixelShader = context.getCachedShader(psCodeHash);
	if (!pixelShader) {
		ShaderCompileOutput output;
		bool psCompiled = compile('f', output, psCode);
		if (!psCompiled) {
			spdlog::error("Failed to compiler pixel shader");
			return ShaderProgramPtr();
		}

		bgfx::ShaderHandle shaderHandle = bgfx::createShader(bgfx::copy(output.binary.data(), output.binary.size()));
		pixelShader = std::make_shared<ShaderHandle>(shaderHandle);

		context.compiledShaders[psCodeHash] = pixelShader;
	}

	if (!vertexShader || !pixelShader)
		return ShaderProgramPtr();

	ShaderProgramPtr shaderProgram = std::make_shared<ShaderProgram>(bgfx::createProgram(vertexShader->handle, pixelShader->handle, false));
	shaderProgram->textureRegisterMap = shaderBuilder.textureRegisterMap;
	return shaderProgram;
}

void MaterialUsageContext::preloadUniforms() {
	const MaterialData &materialData = material.getData();

	for (auto &pair : context.defaultVectorParameters) {
		context.getUniformHandle(pair.first, bgfx::UniformType::Vec4);
	}

	for (auto &pair : materialData.vectorParameters) {
		context.getUniformHandle(pair.first, bgfx::UniformType::Vec4);
	}

	for (auto &pair : materialData.textureSlots) {
		context.getUniformHandle(pair.first, bgfx::UniformType::Sampler);
	}

	for (auto &pair : textureSlots) {
		context.getUniformHandle(pair.first, bgfx::UniformType::Sampler);
	}
}

void MaterialUsageContext::bindUniforms(ShaderProgramPtr program) {
	const MaterialData &materialData = material.getData();

	std::unordered_map<std::string, float4> vectorParameters = context.defaultVectorParameters;
	for (auto &pair : materialData.vectorParameters) {
		vectorParameters[pair.first] = pair.second;
	}

	for (auto &vecParam : vectorParameters) {
		bgfx::UniformHandle handle = context.getUniformHandle(vecParam.first, bgfx::UniformType::Vec4);
		bgfx::setUniform(handle, &vecParam.second.x);
	}

	auto bindTextureSlots = [&](const std::unordered_map<std::string, MaterialTextureSlot> &textureSlots) {
		for (auto &texSlot : textureSlots) {
			bgfx::UniformHandle handle = context.getUniformHandle(texSlot.first, bgfx::UniformType::Sampler);

			TexturePtr texture = texSlot.second.texture;
			if (!texture)
				texture = context.fallbackTexture;

			size_t registerIndex = program->textureRegisterMap[texSlot.first];

			bgfx::setTexture(registerIndex, handle, texture->handle);
		}
	};

	bindTextureSlots(materialData.textureSlots);
	bindTextureSlots(textureSlots);
}

void MaterialUsageContext::setState() { bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS); }

} // namespace gfx
