#include "bgfx/bgfx.h"
#include "bgfx/defines.h"
#include "bgfx/src/bgfx_p.h"
#include "gfx/context.hpp"
#include "gfx/frame.hpp"
#include "gfx/geom.hpp"
#include "gfx/imgui.hpp"
#include "gfx/loop.hpp"
#include "gfx/mesh.hpp"
#include "gfx/paths.hpp"
#include "gfx/shaderc.hpp"
#include "gfx/types.hpp"
#include "gfx/view.hpp"
#include "gfx/window.hpp"
#include "imgui.h"
#include "spdlog/spdlog.h"
#include <SDL2/SDL_events.h>
#include <bx/string.h>
#include <cassert>
#include <cstring>
#include <exception>
#include <string>

using namespace gfx;

struct App {
	gfx::Window window;
	gfx::Context context;
	gfx::Loop loop;

	void init(const char *hostElement) {
		spdlog::info("sandbox Init");
		window.init();
		gfx::ContextCreationOptions contextOptions;
		contextOptions.renderer = RendererType::OpenGL;
		contextOptions.overrideNativeWindowHandle = const_cast<char *>(hostElement);
		context.init(window, contextOptions);
	}

	void tick() {
		bool quit = false;
		while (!quit) {
			std::vector<SDL_Event> events;
			window.pollEvents(events);
			for (auto &event : events) {
				if (event.type == SDL_QUIT)
					quit = true;
			}

			float deltaTime;
			if (loop.beginFrame(1.0f / 120.0f, deltaTime)) {

				gfx::FrameRenderer frame(context, gfx::FrameInputs(deltaTime, loop.getAbsoluteTime(), 0, events));
				frame.begin();

				gfx::View &view = frame.pushMainOutputView();
				bgfx::setViewClear(view.id, BGFX_CLEAR_COLOR, colorToRGBA(Color(10, 10, 10, 255)));
				frame.popView();

				frame.end();
			}
		}
	}
};

int main() {
	App instance;
	instance.init(nullptr);
	instance.tick();
	return 0;
}
