#include "gfx/context.hpp"
#include "gfx/drawable.hpp"
#include "gfx/enums.hpp"
#include "gfx/features/base_color.hpp"
#include "gfx/features/debug_color.hpp"
#include "gfx/features/transform.hpp"
#include "gfx/geom.hpp"
#include "gfx/linalg.hpp"
#include "gfx/loop.hpp"
#include "gfx/mesh.hpp"
#include "gfx/moving_average.hpp"
#include "gfx/paths.hpp"
#include "gfx/renderer.hpp"
#include "gfx/texture.hpp"
#include "gfx/texture_file.hpp"
#include "gfx/types.hpp"
#include "gfx/utils.hpp"
#include "gfx/view.hpp"
#include "gfx/window.hpp"
#include "linalg/linalg.h"
#include "spdlog/spdlog.h"
#include <SDL_events.h>
#include <SDL_video.h>
#include <cassert>
#include <cstring>
#include <exception>
#include <memory>
#include <random>
#include <string>
#include <vector>

using namespace gfx;

#if GFX_EMSCRIPTEN
#include <emscripten/html5.h>
void osYield() { emscripten_sleep(0); }
#else
void osYield() {}
#endif

struct VertexPC {
	float position[3];
	float color[4];

	VertexPC() = default;
	VertexPC(const geom::VertexPNT &other) {
		setPosition(*(float3 *)other.position);
		setColor(float4(1, 1, 1, 1));
	}

	void setPosition(const float3 &v) { memcpy(position, &v.x, sizeof(float) * 3); }
	void setColor(const float4 &v) { memcpy(color, &v.x, sizeof(float) * 4); }

	static std::vector<MeshVertexAttribute> getAttributes() {
		std::vector<MeshVertexAttribute> attribs;
		attribs.emplace_back("position", 3, VertexAttributeType::Float32);
		attribs.emplace_back("color", 4, VertexAttributeType::Float32);
		return attribs;
	}
};

template <typename T> std::vector<T> convertVertices(const std::vector<geom::VertexPNT> &input) {
	std::vector<T> result;
	for (auto &vert : input)
		result.emplace_back(vert);
	return result;
}

template <typename T> MeshPtr createMesh(const std::vector<T> &verts, const std::vector<geom::GeneratorBase::index_t> &indices) {
	MeshPtr mesh = std::make_shared<Mesh>();
	MeshFormat format{
		.vertexAttributes = T::getAttributes(),
	};
	mesh->update(format, verts.data(), verts.size() * sizeof(T), indices.data(), indices.size() * sizeof(geom::GeneratorBase::index_t));
	return mesh;
}

struct App {
	Window window;
	Loop loop;
	Context context;

	MeshPtr sphereMesh;
	MeshPtr cubeMesh;
	ViewPtr view;
	std::shared_ptr<Renderer> renderer;
	DrawQueue drawQueue;

	std::vector<TexturePtr> textures;

	PipelineSteps pipelineSteps;

	App() {}

	void init(const char *hostElement) {
		spdlog::debug("sandbox Init");
		window.init(WindowCreationOptions{.width = 512, .height = 512});

		gfx::ContextCreationOptions contextOptions = {};
		contextOptions.overrideNativeWindowHandle = const_cast<char *>(hostElement);
		contextOptions.debug = true;
		context.init(window, contextOptions);

		renderer = std::make_shared<Renderer>(context);

		FeaturePtr blendFeature = std::make_shared<Feature>();
		blendFeature->state.set_blend(BlendState{
			.color = BlendComponent::AlphaPremultiplied,
			.alpha = BlendComponent::Opaque,
		});

		pipelineSteps.emplace_back(makeDrawablePipelineStep(RenderDrawablesStep{
			.features =
				{
					features::Transform::create(),
					features::BaseColor::create(),
					blendFeature,
				},
			.sortMode = SortMode::BackToFront,
		}));

		view = std::make_shared<View>();
		view->proj = ViewPerspectiveProjection{};
		view->view = linalg::lookat_matrix(float3(0, 5, 20), float3(0, 0, 0.0f), float3(0, 1, 0));

		geom::CubeGenerator sphere;
		sphere.generate();

		sphereMesh = createMesh(sphere.vertices, sphere.indices);

		std::vector<std::string> texturePaths = {
			"assets/drawing.png",
			// "assets/pattern-g4073e58ef_1280.png",
			// "assets/abstract-g9e60d84ae_1280.jpg",
		};
		for (auto path : texturePaths) {
			auto texture = textureFromFile(resolveDataPath(path).string().c_str());
			textures.push_back(texture);
		}

		rnd.seed(time(nullptr));
		buildDrawables();
	}

	std::default_random_engine rnd;
	struct DrawThing {
		DrawablePtr drawable;
		float2 coord;
		float4 baseColor;
		float3 spinVel;
		float3 rot{};

		DrawThing(float2 coord, MeshPtr mesh, TexturePtr texture, std::default_random_engine &rnd) : coord(coord) {
			std::uniform_real_distribution<float> dist(0.0f, 1.0f);
			std::uniform_real_distribution<float> sdist(-1.0f, 1.0f);
			baseColor = float4(dist(rnd), dist(rnd), dist(rnd), 1.0f);
			spinVel = float3(sdist(rnd), sdist(rnd), sdist(rnd));

			drawable = std::make_shared<Drawable>(mesh);
			// drawable->parameters.texture.insert_or_assign("baseColor", TextureParameter(texture));
		}
		DrawThing(DrawThing &&other) = default;
		void update(float time, float deltaTime) {

			drawable->transform = linalg::translation_matrix(float3(coord.x, 0.0f, coord.y));

			rot += spinVel * deltaTime;
			float4 rotQuat = linalg::rotation_quat(float3(1, 0, 0), rot.x);
			rotQuat = linalg::qmul(rotQuat, linalg::rotation_quat(float3(0, 0, 1), rot.z));
			rotQuat = linalg::qmul(rotQuat, linalg::rotation_quat(float3(0, 1, 0), rot.y));
			drawable->transform = linalg::mul(drawable->transform, linalg::rotation_matrix(rotQuat));

			drawable->parameters.basic.insert_or_assign("baseColor", baseColor);
		}
	};
	std::vector<DrawThing> things;
	std::vector<DrawablePtr> testDrawables;
	void buildDrawables() {
		testDrawables.clear();
		things.clear();

		std::uniform_int_distribution<int> textureIndexDist(0, textures.size() - 1);

		int2 testGridDim = {32, 32};
		for (size_t y = 0; y < testGridDim.y; y++) {
			float fy = (y - float(testGridDim.y) / 2.0f) * 2.0f;
			for (size_t x = 0; x < testGridDim.x; x++) {
				float fx = (x - float(testGridDim.x) / 2.0f) * 2.0f;

				auto texture = textures[textureIndexDist(rnd)];

				things.emplace_back(float2(fx, fy), sphereMesh, texture, rnd);
				testDrawables.push_back(things.back().drawable);
			}
		}
	}

	void renderFrame(float time, float deltaTime) {
		for (auto &thing : things) {
			thing.update(time, deltaTime);
		}

		for (auto &drawable : testDrawables) {
			drawQueue.add(drawable);
		}

		renderer->render(drawQueue, view, pipelineSteps);
	}

	void runMainLoop() {
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
				context.beginFrame();
				renderer->swapBuffers();
				drawQueue.clear();

				renderFrame(loop.getAbsoluteTime(), deltaTime);

				context.endFrame();

				static MovingAverage delaTimeMa(32);
				delaTimeMa.add(deltaTime);

				static float fpsCounter = 0.0f;
				fpsCounter += deltaTime;
				if (fpsCounter >= 1.0f) {
					spdlog::info("Average FPS: {:0.01f}", 1.0f / delaTimeMa.getAverage());
					fpsCounter = 0.0f;
				}
			} else {
				osYield();
			}
		}
	}
};

int main() {
	App instance;
	instance.init(nullptr);
	instance.runMainLoop();
	return 0;
}
