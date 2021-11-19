#pragma once
#include "test_data.hpp"
#include <functional>
#include <gfx/capture.hpp>
#include <gfx/context.hpp>
#include <gfx/test_platform_id.hpp>
#include <gfx/tests/test_data.hpp>
#include <gfx/window.hpp>

#define CHECK_FRAME(_id) CHECK(checkFrame(_id))

struct DrawFixture {
	gfx::Window window;
	gfx::Context context;
	gfx::TestData testData;

	std::shared_ptr<gfx::SingleFrameCapture> capture = gfx::SingleFrameCapture::create();

	DrawFixture() {
		window.init();
		context.init(window);
		context.addCapture(capture);
		testData = gfx::TestData(gfx::TestPlatformId::get(context));
	}

	void frameWithCaptureSync() {
		gfx::FrameCaptureSync _(context);
		bgfx::frame();
	}

	bool checkFrame(const char *id, float tolerance = 0) {
		gfx::TestFrame frame(*capture.get());
		return testData.checkFrame(id, frame, tolerance);
	}

	bool pollEvents() {
		std::vector<SDL_Event> events;
		window.pollEvents(events);
		for (const auto &event : events) {
			if (event.type == SDL_QUIT)
				return false;
		}
		return true;
	}
};