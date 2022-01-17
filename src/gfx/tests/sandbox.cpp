#include "gfx/context.hpp"
#include "gfx/drawable.hpp"
#include "gfx/enums.hpp"
#include "gfx/frame.hpp"
#include "gfx/geom.hpp"
#include "gfx/hasherxxh128.hpp"
#include "gfx/linalg.hpp"
#include "gfx/loop.hpp"
#include "gfx/mesh.hpp"
#include "gfx/paths.hpp"
#include "gfx/types.hpp"
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
#include <string>
#include <vector>

using namespace gfx;

struct App {
	gfx::Window window;
	gfx::Context context;
	gfx::Loop loop;

	ViewPtr view;

	void init(const char *hostElement) {
		spdlog::debug("sandbox Init");
		window.init();

		gfx::ContextCreationOptions contextOptions;
		contextOptions.overrideNativeWindowHandle = const_cast<char *>(hostElement);
		context.init(window, contextOptions);

		view = std::make_shared<View>();
		view->view = linalg::lookat_matrix(float3(0, 6.0f, -8.0f), float3(0, 0, 0), float3(0, 1, 0));
		view->proj = ViewPerspectiveProjection{
			degToRad(45.0f),
			FovDirection::Horizontal,
		};
	}

	void tick() {
		gfx::FrameRenderer frame(context);
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
				frame.begin(gfx::FrameInputs(deltaTime, loop.getAbsoluteTime(), 0, events));

				// FRAME

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
