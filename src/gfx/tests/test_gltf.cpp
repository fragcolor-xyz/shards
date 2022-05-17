#include "./data.hpp"
#include "./renderer.hpp"
#include "renderer_utils.hpp"
#include <gfx/context.hpp>
#include <gfx/drawable.hpp>
#include <gfx/gltf/gltf.hpp>
#include <gfx/mesh.hpp>
#include <gfx/paths.hpp>
#include <spdlog/fmt/fmt.h>
#include <catch2/catch_all.hpp>

using namespace gfx;

TEST_CASE("Fox.glb", "[glTF]") {
  auto testPath = gfx::resolveDataPath("glTF-Sample-Models/2.0/fox/glTF-Binary/Fox.glb");

  DrawableHierarchyPtr gltfScene;
  CHECK_NOTHROW(gltfScene = loadGlTF(testPath.string().c_str()));
  CHECK(gltfScene);

  std::shared_ptr<TestContext> context = createTestContext();

  auto testRenderer = std::make_shared<TestRenderer>(context);
  testRenderer->createRenderTarget(int2(1280, 720));
  Renderer &renderer = *testRenderer->renderer.get();

  ViewPtr view = createTestProjectionView();
  DrawQueue queue;
  queue.add(gltfScene);

  TEST_RENDER_LOOP(testRenderer) { renderer.render(queue, view, createTestPipelineSteps()); };
  CHECK(testRenderer->checkFrame("fox.glb"));
}
