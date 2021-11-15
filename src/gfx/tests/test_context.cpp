#include "gfx/capture.hpp"
#include "gfx/context.hpp"
#include "gfx/types.hpp"
#include "gfx/window.hpp"
#include <SDL_events.h>
#include <SDL_events.h>
#include <bgfx/bgfx.h>
#include <bgfx/defines.h>
#include <bgfx/platform.h>
#include <bx/os.h>
#include <catch2/catch_all.hpp>
#include <catch2/catch_test_macros.hpp>
#include <memory>
#include <spdlog/spdlog.h>
#include <stdint.h>
#include <vector>

TEST_CASE("Window") {
	SECTION("Default settings") {
		std::shared_ptr<gfx::Window> wnd = std::make_shared<gfx::Window>();

		wnd->init();
		CHECK(wnd->window);

		std::vector<SDL_Event> events;
		wnd->pollEvents(events);

		wnd.reset();
	}

	SECTION("Fullscreen") {
		std::shared_ptr<gfx::Window> wnd = std::make_shared<gfx::Window>();

		gfx::WindowCreationOptions options;
		options.fullscreen = true;
		CHECK_NOTHROW(wnd->init(options));
		CHECK(wnd->window);

		std::vector<SDL_Event> events;
		wnd->pollEvents(events);

		wnd.reset();
	}
}

struct WindowFixture {
	gfx::Window window;
	WindowFixture() { window.init(); }
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

TEST_CASE_METHOD(WindowFixture, "Context") {
	gfx::Context context;
	CHECK_NOTHROW(context.init(window));
}

struct ContextFixture : public WindowFixture {
	gfx::Context context;
	ContextFixture() { context.init(window); }
	void frameWithCaptureSync() {
		gfx::FrameCaptureSync _(context);
		bgfx::frame();
	}
};

TEST_CASE_METHOD(ContextFixture, "Capture Frames") {
	std::shared_ptr<gfx::SingleFrameCapture> capture =
		gfx::SingleFrameCapture::create();
	context.addCapture(capture);

	CHECK(capture->frameCounter == UINT32_MAX);
	bx::sleep(100);
	CHECK(capture->frameCounter == UINT32_MAX);

	gfx::Color clearColor = gfx::Color(100, 0, 200, 255);

	bgfx::setViewRect(0, 0, 0, bgfx::BackbufferRatio::Equal);
	bgfx::setViewClear(0,
					   BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH | BGFX_CLEAR_STENCIL,
					   gfx::colorToRGBA(clearColor));
	bgfx::touch(0);
	frameWithCaptureSync();

	CHECK(capture->frameCounter == 0);
	bx::sleep(100);
	CHECK(capture->frameCounter == 0);

	CHECK(capture->format.format == bgfx::TextureFormat::BGRA8);
	CHECK(capture->data.size() >= 4);
	const uint32_t *pixelData = (uint32_t *)capture->data.data();
	gfx::Color firstPixel = gfx::colorFromARGB(pixelData[0]);

	CHECK(firstPixel == clearColor);

	bgfx::touch(0);
	frameWithCaptureSync();
	CHECK(capture->frameCounter == 1);

	pixelData = (uint32_t *)capture->data.data();
	firstPixel = gfx::colorFromARGB(pixelData[0]);

	CHECK(firstPixel == clearColor);
}