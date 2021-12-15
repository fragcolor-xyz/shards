#include "bgfx/bgfx.h"
#include "bgfx/defines.h"
#include "bx/math.h"
#include "gfx/context.hpp"
#include "gfx/drawable.hpp"
#include "gfx/enums.hpp"
#include "gfx/feature.hpp"
#include "gfx/feature_pipeline.hpp"
#include "gfx/features/base_color.hpp"
#include "gfx/features/time.hpp"
#include "gfx/features/transforms.hpp"
#include "gfx/fields.hpp"
#include "gfx/frame.hpp"
#include "gfx/geom.hpp"
#include "gfx/hasherxxh128.hpp"
#include "gfx/imgui.hpp"
#include "gfx/linalg.hpp"
#include "gfx/loop.hpp"
#include "gfx/material.hpp"
#include "gfx/mesh.hpp"
#include "gfx/paths.hpp"
#include "gfx/renderer.hpp"
#include "gfx/shaderc.hpp"
#include "gfx/types.hpp"
#include "gfx/view.hpp"
#include "gfx/window.hpp"
#include "imgui.h"
#include "linalg/linalg.h"
#include "spdlog/spdlog.h"
#include <SDL2/SDL_events.h>
#include <SDL_events.h>
#include <SDL_video.h>
#include <boost/smart_ptr/make_shared_array.hpp>
#include <bx/string.h>
#include <cassert>
#include <cstring>
#include <exception>
#include <memory>
#include <string>
#include <vector>

using namespace gfx;

inline MeshPtr generateSphereMesh() {
	auto result = std::make_shared<Mesh>();
	result->primitiveType = PrimitiveType::TriangleList;
	result->vertexAttributes = gfx::geom::VertexPNT::getAttributes();

	gfx::geom::SphereGenerator gen;
	gen.generate();

	bgfx::VertexLayout layout = gfx::createVertexLayout(result->vertexAttributes);

	const bgfx::Memory *vertMem = bgfx::copy(gen.vertices.data(), gen.vertices.size() * sizeof(gfx::geom::VertexPNT));
	result->vb = bgfx::createVertexBuffer(vertMem, layout);

	const bgfx::Memory *indexMem = bgfx::copy(gen.indices.data(), gen.indices.size() * sizeof(gfx::geom::GeneratorBase::index_t));
	result->ib = bgfx::createIndexBuffer(indexMem);

	return result;
}

struct App {
	gfx::Window window;
	gfx::Context context;
	gfx::Loop loop;

	std::vector<FeaturePtr> features;
	std::vector<DrawablePtr> drawables;

	Renderer renderer;
	ViewPtr view;

	void draw(Drawable &drawable) {}

	void init(const char *hostElement) {
		spdlog::info("sandbox Init");
		window.init();
		gfx::ContextCreationOptions contextOptions;
		contextOptions.renderer = RendererType::None;
		contextOptions.overrideNativeWindowHandle = const_cast<char *>(hostElement);
		context.init(window, contextOptions);

		bgfx::setDebug(BGFX_DEBUG_TEXT | BGFX_DEBUG_PROFILER | BGFX_DEBUG_STATS);

		auto mesh = generateSphereMesh();

		auto customShader = std::make_shared<Feature>();
		{
			auto &code0 = customShader->shaderCode.emplace_back();
			code0.code = R"(
void main(inout MaterialInfo mi) {
	float wave = mix(0.7f, 1.6f, cos(mi.worldPos.x * 0.1f + u_time.x) * 0.5f + 0.5f);
	mi.color *= wave;
}
)";
			code0.dependencies.emplace_back("transform");
			code0.dependencies.emplace_back("outputColor", DependencyType::Before);
		}

		view = std::make_shared<View>();
		view->view = linalg::lookat_matrix(float3(0, 6.0f, -8.0f), float3(0, 0, 0), float3(0, 1, 0));
		view->proj = ViewPerspectiveProjection{
			bx::toRad(45.0f),
			FovDirection::Horizontal,
		};

		size_t gridSize = 64;
		float spacing = 0.4f;
		float scale = 0.2f;
		float totalSize = gridSize * spacing;
		for (size_t zi = 0; zi < gridSize; zi++) {
			for (size_t xi = 0; xi < gridSize; xi++) {
				float fx = float(xi) * spacing - (totalSize / 2.0f);
				float fz = float(zi) * spacing - (totalSize / 2.0f);

				float4x4 transform = linalg::identity;
				transform = linalg::mul(transform, linalg::translation_matrix(float3(fx, 0.0f, fz)));
				transform = linalg::mul(transform, linalg::scaling_matrix(float3(scale)));

				auto mat = std::make_shared<Material>();
				mat->customFeatures.push_back(customShader);
				mat->modify([&](MaterialData &md) {
					float4 &param = md.basicParameters["baseColor"].emplace<float4>();

					float3 hsv = float3(0.0f, 0.8f, 0.5f);
					hsv.x = float(xi) / float(gridSize) + float(zi) / float(gridSize);

					bx::hsvToRgb(&param.x, &hsv.x);
					param.w = 1.0f;
				});

				drawables.push_back(std::make_shared<Drawable>(mesh, transform, mat));
			}
		}

		features.push_back(features::Time::create());
		features.push_back(features::BaseColor::create());
		features.push_back(features::Transform::create());
	}

	void tick() {
		bool quit = false;
		while (!quit) {
			std::vector<SDL_Event> events;
			window.pollEvents(events);
			for (auto &event : events) {
				if (event.type == SDL_WINDOWEVENT) {
					if (event.window.type == SDL_WINDOWEVENT_SIZE_CHANGED) {
					}
				}
				if (event.type == SDL_QUIT)
					quit = true;
			}

			context.resizeMainOutputConditional(window.getDrawableSize());

			float deltaTime;
			if (loop.beginFrame(1.0f / 120.0f, deltaTime)) {
				gfx::FrameRenderer frame(context, gfx::FrameInputs(deltaTime, loop.getAbsoluteTime(), 0, events));
				frame.begin();

				DrawQueue dq;
				for (size_t i = 0; i < drawables.size(); i++) {
					auto &transform = drawables[i]->transform;
					float f = transform.w.x + transform.w.z;
					transform.w.y = bx::cos(f * 0.2f + loop.getAbsoluteTime()) * 0.6f;
					dq.add(drawables[i]);
				}

				Pipeline pipeline = {
					DrawablePass(features),
				};
				std::vector<ViewPtr> views = {view};
				renderer.render(frame, dq, pipeline, views);

				frame.end();
			}
		}
	}
};

int main() {
	App &instance = *new App();
	instance.init(nullptr);
	instance.tick();
	return 0;
}
