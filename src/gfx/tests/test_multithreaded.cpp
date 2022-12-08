// Tests for multi-threaded rendering logic

#include "./context.hpp"
#include "./data.hpp"
#include "./renderer.hpp"
#include "renderer_utils.hpp"
#include <catch2/catch_all.hpp>
#include <gfx/context.hpp>
#include <gfx/drawable.hpp>
#include <gfx/drawables/mesh_drawable.hpp>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>
#include <type_traits>
#include <taskflow/taskflow.hpp>
#include <chrono>

using namespace gfx;

#if GFX_THREADING_SUPPORT
TEST_CASE("Taskflow", "[MT]") {
  tf::Executor executor(4);

  std::atomic_int counter{};

  tf::Taskflow flow;
  auto a = flow.for_each_index(0, 100, 1, [&](size_t index) { counter++; });
  auto b = flow.emplace([&]() { counter = counter * 2; });
  b.succeed(a);

  auto c = flow.emplace([&]() { counter = counter + 1; });
  c.succeed(b);

  executor.run(flow).wait();

  CHECK(counter == 201);
}
#endif

TEST_CASE("Taskflow No Threads", "[MT]") {
  using namespace std::chrono_literals;

  tf::Executor executor(0);

  std::atomic_int counter{};

  tf::Taskflow flow;
  auto a = flow.for_each_index(0, 100, 1, [&](size_t index) { counter++; });
  auto b = flow.emplace([&]() { counter = counter * 2; });
  b.succeed(a);

  auto c = flow.emplace([&]() { counter = counter + 1; });
  c.succeed(b);

  auto task = executor.run(flow);
  executor.loop_until([&]() {
    return task.wait_for(0ms) == std::future_status::ready;
  });

  CHECK(counter == 201);
}

// TEST_CASE("Drawable ownership", "[MT]") {
//   auto testRenderer = createTestRenderer();
//   auto renderer = testRenderer->renderer;

//   MeshPtr meshes[] = {
//       createCubeMesh(),
//       createSphereMesh(),
//   };

//   DrawQueuePtr queue = std::make_shared<DrawQueue>();
//   ViewPtr view = std::make_shared<View>();
//   auto renderSteps = createTestPipelineSteps(queue);

//   constexpr static size_t numFrames = 1000;
//   constexpr static size_t addObjects = 100;
//   std::vector<DrawablePtr> data;
//   for (size_t i = 0; i < numFrames; i++) {
//     testRenderer->context->tick();
//     testRenderer->begin();

//     // Remove some elements
//     for (size_t j = 0; j < data.size();) {
//       size_t ri = (i + j) % 6;
//       if (ri == 0) {
//         data.erase(data.begin() + ri);
//       } else {
//         ++j;
//       }
//     }

//     // Add some new elements
//     for (size_t j = 0; j < addObjects; j++) {
//       data.emplace_back(std::make_shared<MeshDrawable>(meshes[j % 2]));
//     }

//     queue->clear();
//     for (auto &drawable : data) {
//       queue->add(drawable);
//     }
//     renderer->render(view, renderSteps);

//     testRenderer->end();
//   }
// }