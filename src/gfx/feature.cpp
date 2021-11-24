#include "feature.hpp"
#include "bgfx/bgfx.h"
#include "bgfx/defines.h"
#include "env_probe.hpp"
#include "frame.hpp"
#include "geom.hpp"
#include "linalg.hpp"
#include "material.hpp"
#include "material_shader.hpp"
#include "math.hpp"
#include "scene.hpp"
#include "spdlog/fmt/bundled/core.h"
#include "texture.hpp"
#include <cmath>
#include <initializer_list>
#include <memory>
#include <vector>

namespace gfx {

struct DirectionalLightData {
	float color[4];
	float directionIntensity[4];
};

void DirectionalLightsFeature::setupScene(SceneRenderer &sceneRenderer) {
	for (auto &light : sceneRenderer.lights) {
		if (DirectionalLight *dirLight = dynamic_cast<DirectionalLight *>(light.get())) {
			directionalLights.push_back(dirLight);
		}
	}

	std::vector<DirectionalLightData> directionalLightData;
	for (DirectionalLight *light : directionalLights) {
		DirectionalLightData &data = directionalLightData.emplace_back();
		memcpy(data.color, &light->color, sizeof(float) * 4);
		memcpy(data.directionIntensity, &light->direction, sizeof(float) * 3);
		data.directionIntensity[3] = light->intensity;
	}

	size_t numDirectionalLightVecs = 2 * directionalLights.size();
	u_directionalLightData = bgfx::createUniform("u_dirLightData", bgfx::UniformType::Vec4, numDirectionalLightVecs);
	bgfx::setUniform(u_directionalLightData, directionalLightData.data(), numDirectionalLightVecs);
}

void DirectionalLightsFeature::setupMaterial(SceneRenderer &scene, MaterialUsageContext &context) {
	context.staticOptions.numDirectionLights = directionalLights.size();
}

struct PointLightData {
	float colorInnerRadius[4];
	float positionIntensity[4];
	float outerRadius[4];
};

void PointLightsFeature::setupScene(SceneRenderer &sceneRenderer) {
	for (auto &light : sceneRenderer.lights) {
		if (PointLight *dirLight = dynamic_cast<PointLight *>(light.get())) {
			pointLights.push_back(dirLight);
		}
	}

	std::vector<PointLightData> pointLightData;
	for (PointLight *light : pointLights) {
		PointLightData &data = pointLightData.emplace_back();
		memcpy(data.colorInnerRadius, &light->color, sizeof(float) * 3);
		data.colorInnerRadius[3] = light->innerRadius;
		memcpy(data.positionIntensity, &light->position, sizeof(float) * 3);
		data.positionIntensity[3] = light->intensity;
		data.outerRadius[0] = light->outerRadius;
	}

	size_t numPointLightVecs = 3 * pointLights.size();
	u_pointLightData = bgfx::createUniform("u_pointLightData", bgfx::UniformType::Vec4, numPointLightVecs);
	bgfx::setUniform(u_pointLightData, pointLightData.data(), numPointLightVecs);
}

void PointLightsFeature::setupMaterial(SceneRenderer &scene, MaterialUsageContext &context) { context.staticOptions.numPointLights = pointLights.size(); }

static const char *textureEnvLambertTexture = "envLambertTexture";
static const char *textureEnvGGXTexture = "envGGXTexture";
static const char *textureEnvGGXLUTTexture = "envGGXLUTTexture";

void applyFilter(FrameRenderer &frameRenderer, MaterialBuilderContext &mbc, TexturePtr outputTexture, Material &material, int targetMipLevel = 0) {
	gfx::geom::PlaneGenerator gen;
	gen.width = 2.0f;
	gen.height = 2.0f;
	gen.heightSegments = 1;
	gen.widthSegments = 1;
	gen.generate();

	const bgfx::Memory *vertMem = bgfx::copy(gen.vertices.data(), gen.vertices.size() * sizeof(gfx::geom::VertexPNT));
	bgfx::VertexBufferHandle vb = bgfx::createVertexBuffer(vertMem, gfx::geom::VertexPNT::getVertexLayout());
	const bgfx::Memory *indexMem = bgfx::copy(gen.indices.data(), gen.indices.size() * sizeof(gfx::geom::GeneratorBase::index_t));
	bgfx::IndexBufferHandle ib = bgfx::createIndexBuffer(indexMem);

	int2 baseSize = outputTexture->getSize();
	int2 mipSize = int2(baseSize.x >> targetMipLevel, baseSize.y >> targetMipLevel);
	TexturePtr fbTexture = std::make_shared<Texture>(mipSize, outputTexture->getFormat(), BGFX_TEXTURE_RT);
	FrameBufferPtr output = std::make_shared<FrameBuffer>(std::initializer_list<FrameBufferAttachment>{FrameBufferAttachment(fbTexture)});

	View &view = frameRenderer.pushView();
	bgfx::setViewFrameBuffer(view.id, output->handle);
	view.setViewport(gfx::Rect(fbTexture->getSize().x, fbTexture->getSize().y));

	MaterialUsageContext materialContext(mbc);
	materialContext.staticOptions.usageFlags |= MaterialUsageFlags::FlipTexcoordY;
	materialContext.material = material;
	ShaderProgramPtr program = materialContext.getProgram();
	materialContext.setState();
	materialContext.bindUniforms(program);

	bgfx::setIndexBuffer(ib);
	bgfx::setVertexBuffer(0, vb);
	bgfx::submit(view.id, program->handle);

	frameRenderer.popView();

	bgfx::destroy(vb);
	bgfx::destroy(ib);

	View &blitView = frameRenderer.pushView();
	bgfx::blit(blitView.id,									   // view
			   outputTexture->handle, targetMipLevel, 0, 0, 0, // destination mip/x/y/z
			   fbTexture->handle, 0, 0, 0, 0,				   // source mip/x/y/z
			   mipSize.x, mipSize.y, 0);
	frameRenderer.popView();
}

void EnvironmentProbeFeature::runPrecompute(SceneRenderer &sceneRenderer) {
	if (!ggxLUTTexture) {
		precomputeGGXLUT(sceneRenderer);
	}

	for (auto envProbe : sceneRenderer.environmentProbes) {
		if (envProbe->needsRebuild())
			precomputeEnvironmentProbe(sceneRenderer, envProbe);
		firstEnvProbe = envProbe;
		break;
	}
}

void EnvironmentProbeFeature::setupScene(SceneRenderer &sceneRenderer) {
	u_envLambertTexture = bgfx::createUniform(fmt::format("u_{}", textureEnvLambertTexture).c_str(), bgfx::UniformType::Sampler);
	u_envGGXTexture = bgfx::createUniform(fmt::format("u_{}", textureEnvGGXTexture).c_str(), bgfx::UniformType::Sampler);
	u_envGGXLUTTexture = bgfx::createUniform(fmt::format("u_{}", textureEnvGGXLUTTexture).c_str(), bgfx::UniformType::Sampler);
	u_envNumMipLevels = bgfx::createUniform("u_envNumMipLevels", bgfx::UniformType::Vec4);
}

void EnvironmentProbeFeature::precomputeGGXLUT(SceneRenderer &sceneRenderer) {
	int ggxLUTResolution = 128;
	ggxLUTTexture = std::make_shared<Texture>(int2(ggxLUTResolution), bgfx::TextureFormat::RG32F);

	Material filterMaterial;
	filterMaterial.modify([&](MaterialData &data) {
		data.flags = MaterialStaticFlags::Fullscreen;
		data.pixelCode = R"(
#include <filters/specularLUT.sh>
)";
	});

	applyFilter(sceneRenderer.getFrameRenderer(), sceneRenderer.getMaterialBuilderContext(), ggxLUTTexture, filterMaterial);
}

void blitCubeMips(View &view, TexturePtr dest, TexturePtr source, int faceIndex) {
	int baseResolution = source->getSize().x;
	for (size_t mipLevel = 0;; mipLevel++) {
		size_t mipResolution = baseResolution >> mipLevel;
		if (mipResolution <= 0)
			break;
		bgfx::blit(view.id,									// view
				   dest->handle, mipLevel, 0, 0, faceIndex, // destination mip/x/y/z
				   source->handle, mipLevel, 0, 0, 0,		// source mip/x/y/z
				   mipResolution, mipResolution, 0);
	}
}

void EnvironmentProbeFeature::precomputeEnvironmentProbe(SceneRenderer &sceneRenderer, std::shared_ptr<EnvironmentProbe> probe) {
	float4x4 projMatrix = linalg::perspective_matrix(halfPi, 1.0f, 10.f, 1000.0f);

	const int baseResolution = (512);
	const bgfx::TextureFormat::Enum format = bgfx::TextureFormat::RGBA32F;

	float3 up(0, 1, 0);
	float3 right(1, 0, 0);
	struct CaptureView {
		std::string name;
		float4x4 viewMatrix;
	} views[6] = {
		{"front", linalg::rotation_matrix(linalg::rotation_quat(up, pi))},
		{"back", linalg::rotation_matrix(linalg::rotation_quat(up, 0.0f))},
		{"right", linalg::rotation_matrix(linalg::rotation_quat(up, -halfPi))},
		{"left", linalg::rotation_matrix(linalg::rotation_quat(up, halfPi))},
		{"top", linalg::rotation_matrix(linalg::qmul(linalg::rotation_quat(right, -halfPi), linalg::rotation_quat(up, pi)))},
		{"bottom", linalg::rotation_matrix(linalg::qmul(linalg::rotation_quat(right, halfPi), linalg::rotation_quat(up, pi)))},
	};
	int cubeFaceMapping[6] = {
		BGFX_CUBE_MAP_POSITIVE_Z, BGFX_CUBE_MAP_NEGATIVE_Z, BGFX_CUBE_MAP_POSITIVE_X,
		BGFX_CUBE_MAP_NEGATIVE_X, BGFX_CUBE_MAP_POSITIVE_Y, BGFX_CUBE_MAP_NEGATIVE_Y,
	};

	bgfx::TextureHandle captureColorTextureHandle = bgfx::createTexture2D(baseResolution, baseResolution, true, 0, format, BGFX_TEXTURE_RT);
	auto captureColorTexture = std::make_shared<Texture>(int2(baseResolution), format, captureColorTextureHandle, TextureType::Texture2D);
	auto captureDepthStencilTexture = std::make_shared<Texture>(int2(baseResolution), bgfx::TextureFormat::D24S8, BGFX_TEXTURE_RT);

	std::vector<FeaturePtr> features = sceneRenderer.features;
	features.erase(std::remove_if(features.begin(), features.end(), [&](FeaturePtr feature) { return feature.get() == this; }));

	bgfx::TextureHandle cubeTextureHandle = bgfx::createTextureCube(baseResolution, true, 1, format);
	bgfx::setName(cubeTextureHandle, "Probe cube capture");
	TexturePtr cubeTexture = std::make_shared<Texture>(int2(baseResolution), format, cubeTextureHandle, TextureType::TextureCube);

	bgfx::Attachment colorAttachment;
	colorAttachment.init(captureColorTexture->handle, bgfx::Access::Write, 0, 1, 0, BGFX_RESOLVE_AUTO_GEN_MIPS);
	bgfx::Attachment depthAttachment;
	depthAttachment.init(captureDepthStencilTexture->handle);

	FrameRenderer &frameRenderer = sceneRenderer.getFrameRenderer();
	size_t captureViewIndex = 0;
	for (CaptureView &captureView : views) {
		auto frameBuffer = std::make_shared<FrameBuffer>(std::initializer_list<FrameBufferAttachment>{colorAttachment, depthAttachment});

		View &view = frameRenderer.pushView();
		view.setViewport(Rect(baseResolution, baseResolution));
		bgfx::setViewFrameBuffer(view.id, frameBuffer->handle);
		bgfx::setViewClear(view.id, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL);
		std::string viewName = fmt::format("{} - precomputeEnvironmentProbe", captureView.name);
		bgfx::setViewName(view.id, viewName.c_str());

		bgfx::Transform transformCache;
		bgfx::allocTransform(&transformCache, 2);
		gfx::packFloat4x4(captureView.viewMatrix, transformCache.data + 16 * 0);
		gfx::packFloat4x4(projMatrix, transformCache.data + 16 * 1);
		bgfx::setViewTransform(view.id, transformCache.data + 16 * 0, transformCache.data + 16 * 1);

		sceneRenderer.renderView(features);
		frameRenderer.popView();

		View &blitView = frameRenderer.pushView();
		blitCubeMips(blitView, cubeTexture, captureColorTexture, cubeFaceMapping[captureViewIndex]);
		frameRenderer.popView();

		captureViewIndex++;
	}

	int2 longLatMapResolution = int2(baseResolution * 2, baseResolution);

	probe->lambertTexture = std::make_shared<Texture>(longLatMapResolution, format);
	probe->ggxTexture = std::make_shared<Texture>(longLatMapResolution, format, 0, true);

	Material diffuseFilterMaterial;
	diffuseFilterMaterial.modify([&](MaterialData &data) {
		data.flags = MaterialStaticFlags::Fullscreen;
		data.pixelCode = R"(
#include <filters/diffuse.sh>
)";
		data.textureSlots["filterInput"].texture = cubeTexture;
		data.vectorParameters["filterInputDimensions"] = float4(cubeTexture->getSize().x, cubeTexture->getSize().y, 0, 0);
	});
	applyFilter(frameRenderer, sceneRenderer.getMaterialBuilderContext(), probe->lambertTexture, diffuseFilterMaterial);

	Material specularFilterMaterial;
	specularFilterMaterial.modify([&](MaterialData &data) {
		data.flags = MaterialStaticFlags::Fullscreen;
		data.pixelCode = R"(
#include <filters/specular.sh>
)";
		data.textureSlots["filterInput"].texture = cubeTexture;
		data.vectorParameters["filterInputDimensions"] = float4(cubeTexture->getSize().x, cubeTexture->getSize().y, 0, 0);
	});

	size_t specularBaseResolution = std::min(longLatMapResolution.x, longLatMapResolution.y);
	size_t numSpecularMipLevels = std::floor(std::log2(specularBaseResolution)) + 1;
	for (size_t mipLevel = 0; mipLevel < numSpecularMipLevels; mipLevel++) {
		float roughness = mipLevel / float(numSpecularMipLevels - 1);
		specularFilterMaterial.modify([&](MaterialData &data) { data.vectorParameters["roughness"] = float4(roughness); });
		applyFilter(frameRenderer, sceneRenderer.getMaterialBuilderContext(), probe->ggxTexture, specularFilterMaterial, mipLevel);
	}

	probe->numMips = numSpecularMipLevels;
}

void EnvironmentProbeFeature::setupMaterial(SceneRenderer &scene, MaterialUsageContext &context) {
	if (!firstEnvProbe)
		return;
	if ((context.material.getData().flags & MaterialStaticFlags::Lit) == 0)
		return;

	context.textureSlots[textureEnvLambertTexture].texture = firstEnvProbe->lambertTexture;
	context.textureSlots[textureEnvGGXTexture].texture = firstEnvProbe->ggxTexture;
	context.textureSlots[textureEnvGGXLUTTexture].texture = ggxLUTTexture;
	context.staticOptions.hasEnvironmentLight = true;
}

void EnvironmentProbeFeature::setupState(SceneRenderer &scene, MaterialUsageContext &context) {
	if (!firstEnvProbe)
		return;
	if ((context.material.getData().flags & MaterialStaticFlags::Lit) == 0)
		return;

	float4 numMipLevels = float4(float(firstEnvProbe->numMips));
	bgfx::setUniform(u_envNumMipLevels, &numMipLevels);
}

} // namespace gfx