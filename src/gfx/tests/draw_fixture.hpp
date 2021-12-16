#pragma once
#include "bgfx/bgfx.h"
#include "gfx/mesh.hpp"
#include "gfx/tests/img_compare.hpp"
#include "img_compare.hpp"
#include <functional>
#include <gfx/capture.hpp>
#include <gfx/context.hpp>
#include <gfx/geom.hpp>
#include <gfx/test_platform_id.hpp>
#include <gfx/tests/img_compare.hpp>
#include <gfx/view.hpp>
#include <gfx/window.hpp>
#include <spdlog/spdlog.h>

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
		gfx::CompareRejection rejection;
		if (!testData.checkFrame(id, frame, tolerance, &rejection)) {
			spdlog::error("Test frame \"{}\" comparison failed", id);
			spdlog::error("at ({}, {}) A:{} != B:{} (component {})", rejection.position.x, rejection.position.y, rejection.a, rejection.b, rejection.component);
		}
		return true;
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
	bgfx::VertexLayout layout = gfx::createVertexLayout(gfx::geom::VertexPNT::getAttributes());
	vb = bgfx::createVertexBuffer(vertMem, layout);
	const bgfx::Memory *indexMem = bgfx::copy(gen.indices.data(), gen.indices.size() * sizeof(gfx::geom::GeneratorBase::index_t));
	ib = bgfx::createIndexBuffer(indexMem);
}

inline gfx::MeshPtr generateSphereMesh() {
	auto result = std::make_shared<gfx::Mesh>();
	result->primitiveType = gfx::PrimitiveType::TriangleList;
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

inline void generateFullscreenQuad(bgfx::VertexBufferHandle &vb, bgfx::IndexBufferHandle &ib) {
	gfx::geom::PlaneGenerator gen;
	gen.width = 2.0f;
	gen.height = 2.0f;
	gen.heightSegments = 1;
	gen.widthSegments = 1;
	gen.generate();

	const bgfx::Memory *vertMem = bgfx::copy(gen.vertices.data(), gen.vertices.size() * sizeof(gfx::geom::VertexPNT));
	bgfx::VertexLayout layout = gfx::createVertexLayout(gfx::geom::VertexPNT::getAttributes());
	vb = bgfx::createVertexBuffer(vertMem, layout);
	const bgfx::Memory *indexMem = bgfx::copy(gen.indices.data(), gen.indices.size() * sizeof(gfx::geom::GeneratorBase::index_t));
	ib = bgfx::createIndexBuffer(indexMem);
}
