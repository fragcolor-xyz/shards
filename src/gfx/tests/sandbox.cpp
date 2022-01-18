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
	Window window;
	Loop loop;
	Context context;

	MeshPtr sphereMesh;
	ViewPtr view;

	void init(const char *hostElement) {
		spdlog::debug("sandbox Init");
		window.init();

		gfx::ContextCreationOptions contextOptions = {};
		contextOptions.overrideNativeWindowHandle = const_cast<char *>(hostElement);
		context.init(window, contextOptions);

		view = std::make_shared<View>();
		view->view = linalg::lookat_matrix(float3(0, 6.0f, -8.0f), float3(0, 0, 0), float3(0, 1, 0));
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
	}

	void renderFrame(gfx::FrameRenderer &frame) {
		sphereMesh->createContextDataCondtional(&context);

		WGPUDevice device = context.wgpuDevice;
		WGPUCommandEncoderDescriptor desc = {};
		WGPUCommandEncoder commandEncoder = wgpuDeviceCreateCommandEncoder(device, &desc);

		WGPURenderPassDescriptor passDesc = {};
		passDesc.colorAttachmentCount = 1;
		WGPURenderPassColorAttachment mainAttach = {};
		mainAttach.clearColor = WGPUColor{.r = 1.0f, .g = 0.0f, .b = 1.0f, .a = 1.0f};
		mainAttach.loadOp = WGPULoadOp_Clear;
		mainAttach.view = wgpuSwapChainGetCurrentTextureView(context.wgpuSwapchain);
		mainAttach.storeOp = WGPUStoreOp_Store;
		passDesc.colorAttachments = &mainAttach;
		passDesc.colorAttachmentCount = 1;

		WGPURenderPassEncoder pass = wgpuCommandEncoderBeginRenderPass(commandEncoder, &passDesc);
		wgpuRenderPassEncoderEndPass(pass);

		WGPUCommandBufferDescriptor cmdBufDesc = {};
		WGPUCommandBuffer cmdBuf = wgpuCommandEncoderFinish(commandEncoder, &cmdBufDesc);

		wgpuQueueSubmit(context.wgpuQueue, 1, &cmdBuf);
	}

	void runMainLoop() {
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
				renderFrame(frame);

				frame.end();
				context.sync();
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
