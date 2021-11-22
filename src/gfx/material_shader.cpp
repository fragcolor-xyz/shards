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
		{"roughness", float4(1.0f)},
		{"metallicness", float4(0.0f)},
	};
}

MaterialBuilderContext::~MaterialBuilderContext() {
	for (auto &uniform : uniformsCache) {
		bgfx::destroy(uniform.second);
	}
	uniformsCache.clear();
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

struct StaticHashVisitor {
	template <typename T, typename THasher> void operator()(const T &val, THasher &hasher) { val.hashStatic(hasher); }
};

ShaderProgramPtr MaterialUsageContext::getProgram() {
	// Hash the external parameters
	HasherXXH128<StaticHashVisitor> hasher;
	hasher(*this);
	Hash128 materialContextHash = hasher.getDigest();

	ShaderProgramPtr program = compileProgram();
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

	void assignTextureSlots(const MaterialData &materialData) {
		size_t textureCounter = 0;
		for (auto &textureSlot : materialData.textureSlots) {
			if (textureSlot.first == MaterialBuiltin::baseColorTexture) {
				defines.push_back(fmt::format("GFX_BASE_COLOR_TEXTURE={}", textureSlot.second.texCoordIndex));
			} else if (textureSlot.first == MaterialBuiltin::normalTexture) {
				defines.push_back(fmt::format("GFX_NORMAL_TEXTURE={}", textureSlot.second.texCoordIndex));
			} else if (textureSlot.first == MaterialBuiltin::metalicRoughnessTexture) {
				defines.push_back(fmt::format("GFX_METALLIC_ROUGHNESS_TEXTURE={}", textureSlot.second.texCoordIndex));
			} else if (textureSlot.first == MaterialBuiltin::emissiveTexture) {
				defines.push_back(fmt::format("GFX_EMISSIVE_TEXTURE={}", textureSlot.second.texCoordIndex));
			}

			defines.push_back(fmt::format("u_{}_register={}", textureSlot.first, textureCounter));
			defines.push_back(fmt::format("u_{}_texcoord=v_texcoord{}", textureSlot.first, textureSlot.second.texCoordIndex));
			textureRegisterMap[textureSlot.first] = textureCounter;
			textureCounter++;
		}
	}

	void setupLighting(const MaterialData &materialData, const StaticMaterialOptions &staticOptions) {
		defines.push_back(fmt::format("GFX_MAX_DIR_LIGHTS={}", staticOptions.numDirectionLights));
		defines.push_back(fmt::format("GFX_MAX_POINT_LIGHTS={}", staticOptions.numPointLights));
	}

	void build(const MaterialData &materialData, const StaticMaterialOptions &staticOptions) {
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

		addVarying("texcoord0", "vec2");
		addVarying("normal", "vec3");
		addVarying("tangent", "vec3");
		addVarying("color0", "vec4");
		addVarying("worldPosition", "vec3");

		assignTextureSlots(materialData);

		if (materialData.flags & MaterialStaticFlags::Lit) {
			setupLighting(materialData, staticOptions);
			defines.push_back("GFX_LIT=1");
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
	shaderBuilder.build(material.getData(), staticOptions);

	std::string varyings = shaderBuilder.varyings.str();
	std::string vsCode = shaderBuilder.vsCode.str();
	std::string psCode = shaderBuilder.psCode.str();

	HasherXXH128 hasher;
	hasher(varyings);
	hasher(vsCode);
	Hash128 vsCodeHash = hasher.getDigest();

	hasher.reset();
	hasher(varyings);
	hasher(psCode);
	Hash128 psCodeHash = hasher.getDigest();

	// TODO
	(void)vsCodeHash;
	(void)psCodeHash;

	spdlog::info("Shader varyings:\n{}", varyings);

	auto compile = [&](char type, ShaderCompileOutput &output, std::string code) {
		IShaderCompiler &shaderCompiler = context.context.shaderCompilerModule->getShaderCompiler();
		ShaderCompileOptions options;
		options.shaderType = type;
		options.defines = shaderBuilder.defines;
		options.setCompatibleForContext(context.context);
		options.verbose = false;
		options.debugInformation = true;
		options.optimize = true;
		options.disasm = true;

		spdlog::info("Shader source:\n{}", code);
		return shaderCompiler.compile(options, output, varyings.c_str(), code.data(), code.length());
	};

	ShaderCompileOutput vsOutput, psOutput;
	bool vsCompiled = compile('v', vsOutput, vsCode);
	if (!vsCompiled) {
		spdlog::error("Failed to compiler vertex shader");
		return ShaderProgramPtr();
	}

	bool psCompiled = compile('f', psOutput, psCode);
	if (!psCompiled) {
		spdlog::error("Failed to compiler pixel shader");
		return ShaderProgramPtr();
	}

	bgfx::ShaderHandle vs = bgfx::createShader(bgfx::copy(vsOutput.binary.data(), vsOutput.binary.size()));
	bgfx::ShaderHandle ps = bgfx::createShader(bgfx::copy(psOutput.binary.data(), psOutput.binary.size()));
	return std::make_shared<ShaderProgram>(bgfx::createProgram(vs, ps, true));
}

void MaterialUsageContext::bindUniforms() {
	const MaterialData &materialData = material.getData();

	std::unordered_map<std::string, float4> vectorParameters = context.defaultVectorParameters;
	for (auto &pair : materialData.vectorParameters) {
		vectorParameters[pair.first] = pair.second;
	}

	for (auto &vecParam : vectorParameters) {
		bgfx::UniformHandle handle = context.getUniformHandle(vecParam.first, bgfx::UniformType::Vec4);
		bgfx::setUniform(handle, &vecParam.second.x);
	}

	for (auto &texSlot : materialData.textureSlots) {
		bgfx::UniformHandle handle = context.getUniformHandle(texSlot.first, bgfx::UniformType::Sampler);

		TexturePtr texture = texSlot.second.texture;
		if (!texture)
			texture = context.fallbackTexture;

		size_t registerIndex = textureRegisterMap[texSlot.first];

		bgfx::setTexture(registerIndex, handle, texture->handle);
	}
}

void MaterialUsageContext::setState() { bgfx::setState(BGFX_STATE_WRITE_RGB | BGFX_STATE_WRITE_A | BGFX_STATE_WRITE_Z | BGFX_STATE_DEPTH_TEST_LESS); }

} // namespace gfx
