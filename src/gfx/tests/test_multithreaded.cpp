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
