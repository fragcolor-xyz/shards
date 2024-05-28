#include <shards/core/platform.hpp>

#if SH_WINDOWS
#include <catch2/catch_all.hpp>
#include <shards/log/log.hpp>
#include "renderer.hpp"
#include "renderer_utils.hpp"
#include <shards/gfx/drawables/mesh_drawable.hpp>
#include <Windows.h>
#include <Psapi.h>
#include <algorithm>

using namespace gfx;

inline void dumpMemoryStats(PROCESS_MEMORY_COUNTERS pmc) { SPDLOG_INFO("WorkingSet: {}", pmc.WorkingSetSize); }

TEST_CASE("Test Leaks", "[General]") {
  HANDLE hProcess;
  PROCESS_MEMORY_COUNTERS pmc{.cb = sizeof(pmc)};
  PROCESS_MEMORY_COUNTERS pmc1{.cb = sizeof(pmc)};
  hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, GetCurrentProcessId());

  // Pre-warm
  { auto testRenderer = createTestRenderer(); }

  GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc));
  SPDLOG_INFO("Memory before instances:");
  dumpMemoryStats(pmc);

  std::vector<uint64_t> memLog;

  MeshPtr mesh = createSphereMesh();
  DrawablePtr drawable = std::make_shared<MeshDrawable>(mesh);
  ViewPtr view = std::make_shared<View>();

  DrawQueuePtr queue = std::make_shared<DrawQueue>();
  queue->add(drawable);

  PipelineSteps steps = createTestPipelineSteps(queue);

  // Run many times until memory allocators stabilize and we can see memory being freed
  const size_t goodRunThreshold = 16;
  size_t anyMemoryReductions{};
  for (size_t i = 0; i < 128; i++) {
    {
      auto testRenderer = createTestRenderer();
      for (size_t l = 0; l < 60; l++) {
        testRenderer->begin();
        testRenderer->renderer->render(view, steps);
        testRenderer->end();
      }
    }

    GetProcessMemoryInfo(hProcess, &pmc1, sizeof(pmc1));
    SPDLOG_INFO("Memory after instances:");
    dumpMemoryStats(pmc1);

    auto delta = std::abs(int64_t(pmc1.WorkingSetSize) - int64_t(pmc.WorkingSetSize));
    memLog.push_back(delta);
    if (i > 0 && delta < memLog[i - 1]) {
      if (++anyMemoryReductions > goodRunThreshold)
        break;
    }
  }

  SPDLOG_INFO("Listing all runs:");
  for (size_t i = 0; i < memLog.size(); i++) {
    size_t deltaMb = memLog[i] / 1000 / 1000;
    SPDLOG_INFO("[{}] {}MB delta", i, deltaMb);
  }

  CHECK(anyMemoryReductions > goodRunThreshold);
}

#endif
