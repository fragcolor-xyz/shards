#include "gfx/context.hpp"
#include "gfx/drawable.hpp"
#include "gfx/enums.hpp"
#include "gfx/geom.hpp"
#include "gfx/linalg.hpp"
#include "gfx/loop.hpp"
#include "gfx/mesh.hpp"
#include "gfx/renderer.hpp"
#include "gfx/types.hpp"
#include "gfx/view.hpp"
#include "gfx/window.hpp"
#include "gfx/moving_average.hpp"
#include "linalg/linalg.h"
#include "spdlog/spdlog.h"
#include <SDL_events.h>
#include <SDL_video.h>
#include <cassert>
#include <cstring>
#include <exception>
#include <memory>
#include <string>
#include <vector>

using namespace gfx;

struct App {
	Window window;
	Loop loop;
	Context context;

	MeshPtr sphereMesh;
	ViewPtr view;
	std::shared_ptr<Renderer> renderer;
	DrawQueue drawQueue;

	App() {}

	void init(const char *hostElement) {
		spdlog::debug("sandbox Init");
		window.init();

		gfx::ContextCreationOptions contextOptions = {};
		contextOptions.overrideNativeWindowHandle = const_cast<char *>(hostElement);
		contextOptions.debug = true;
		context.init(window, contextOptions);

		view = std::make_shared<View>();
		view->view = linalg::lookat_matrix(float3(0, 100.0f, 100.0f), float3(0, 0, 0), float3(0, 1, 0));
		view->proj = ViewPerspectiveProjection{
			degToRad(45.0f),
			FovDirection::Horizontal,
		};

		geom::SphereGenerator sphereGen;
		sphereGen.generate();
		sphereMesh = std::make_shared<Mesh>();
		Mesh::Format format = {
			.vertexAttributes = geom::VertexPNT::getAttributes(),
		};
		sphereMesh->update(format, sphereGen.vertices.data(), sizeof(geom::VertexPNT) * sphereGen.vertices.size(), sphereGen.indices.data(),
						   sizeof(geom::GeneratorBase::index_t) * sphereGen.indices.size());

		renderer = std::make_shared<Renderer>(context);
	}

	void renderFrame() {
		drawQueue.clear();

		int2 testGridDim = {2048, 128};
		for (size_t y = 0; y < testGridDim.y; y++) {
			float fy = (y - float(testGridDim.y) / 2.0f);
			for (size_t x = 0; x < testGridDim.x; x++) {
				float fx = (x - float(testGridDim.x) / 2.0f);
				auto drawable = std::make_shared<Drawable>(sphereMesh);
				drawable->transform = linalg::translation_matrix(float3(fx, 0.0f, fy));
				drawQueue.add(drawable);
			}
		}

		renderer->render(drawQueue, view);
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
				renderFrame();
				context.endFrame();

				static MovingAverage delaTimeMa(32);
				delaTimeMa.add(deltaTime);

				static float fpsCounter = 0.0f;
				fpsCounter += deltaTime;
				if (fpsCounter >= 1.0f) {
					spdlog::info("Average FPS: {:0.01f}", 1.0f / delaTimeMa.getAverage());
					fpsCounter = 0.0f;
				}
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
