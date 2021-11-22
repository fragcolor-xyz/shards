#pragma once
#include "bgfx/bgfx.h"
#include "test_data.hpp"
#include <functional>
#include <gfx/capture.hpp>
#include <gfx/context.hpp>
#include <gfx/geom.hpp>
#include <gfx/test_platform_id.hpp>
#include <gfx/tests/test_data.hpp>
#include <gfx/view.hpp>
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

inline void generateSphereMesh(bgfx::VertexBufferHandle &vb, bgfx::IndexBufferHandle &ib) {
	gfx::geom::SphereGenerator gen;
	gen.generate();

	const bgfx::Memory *vertMem = bgfx::copy(gen.vertices.data(), gen.vertices.size() * sizeof(gfx::geom::VertexPNT));
	vb = bgfx::createVertexBuffer(vertMem, gfx::geom::VertexPNT::getVertexLayout());
	const bgfx::Memory *indexMem = bgfx::copy(gen.indices.data(), gen.indices.size() * sizeof(gfx::geom::GeneratorBase::index_t));
	ib = bgfx::createIndexBuffer(indexMem);
}

inline void generateFullscreenQuad(bgfx::VertexBufferHandle &vb, bgfx::IndexBufferHandle &ib) {
	gfx::geom::PlaneGenerator gen;
	gen.width = 2.0f;
	gen.height = 2.0f;
	gen.heightSegments = 1;
	gen.widthSegments = 1;
	gen.generate();

	const bgfx::Memory *vertMem = bgfx::copy(gen.vertices.data(), gen.vertices.size() * sizeof(gfx::geom::VertexPNT));
	vb = bgfx::createVertexBuffer(vertMem, gfx::geom::VertexPNT::getVertexLayout());
	const bgfx::Memory *indexMem = bgfx::copy(gen.indices.data(), gen.indices.size() * sizeof(gfx::geom::GeneratorBase::index_t));
	ib = bgfx::createIndexBuffer(indexMem);
}

inline void setWorldViewProj(gfx::View &view, gfx::float3 cameraLocation = gfx::float3(0, 0, -5)) {
	bgfx::Transform transformCache;

	gfx::float4x4 modelMtx = linalg::identity;
	gfx::float4x4 projMtx = linalg::perspective_matrix(45.0f, float(view.getSize().x) / view.getSize().y, 0.01f, 100.0f);
	gfx::float4x4 viewMtx = linalg::translation_matrix(gfx::float3(0, 0, -5));

	uint32_t transformCacheIndex = bgfx::allocTransform(&transformCache, 3);
	gfx::packFloat4x4(modelMtx, transformCache.data + 16 * 0);
	gfx::packFloat4x4(viewMtx, transformCache.data + 16 * 1);
	gfx::packFloat4x4(projMtx, transformCache.data + 16 * 2);
	bgfx::setTransform(transformCacheIndex);
	bgfx::setViewTransform(view.id, transformCache.data + 16 * 1, transformCache.data + 16 * 2);
}
