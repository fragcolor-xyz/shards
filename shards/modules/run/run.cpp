/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

#include <shards/core/shared.hpp>
#include <shards/core/params.hpp>
#include <shards/common_types.hpp>

namespace shards {
namespace run_ {
struct Schedule {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::AnyType; }

  PARAM_PARAMVAR(_mesh, "Mesh", "The mesh to run", {SHMesh::MeshType});
  PARAM_PARAMVAR(_wire, "Wire", "The wire to run", {shards::CoreInfo::WireType, shards::CoreInfo::WireVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_mesh), PARAM_IMPL_FOR(_wire));

  PARAM_REQUIRED_VARIABLES()
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);

    if (_mesh->valueType == SHType::None) {
      throw ComposeError("Schedule: Mesh parameter is required");
    }

    return data.inputType;
  }
  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto &sharedMesh = *reinterpret_cast<std::shared_ptr<SHMesh> *>(_mesh->payload.objectValue);
    auto &wire = SHWire::sharedFromRef(_wire->payload.wireValue);
    sharedMesh->schedule(wire, SHVar{});

    // Set here as well in case the wire is scheduled on a running mesh
    wire->context->parent = context;
    return input;
  }
};

struct Run {
  Run() { _detached = Var(false); }

  static SHTypesInfo inputTypes() { return shards::CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::BoolType; }

  PARAM_PARAMVAR(_mesh, "Mesh", "The mesh to run", {SHMesh::MeshType});
  PARAM_PARAMVAR(_tickTime, "TickTime", "Time per frame",
                 {shards::CoreInfo::NoneType, shards::CoreInfo::FloatType, shards::CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_iterations, "Iterations", "Number of iterations",
                 {shards::CoreInfo::NoneType, shards::CoreInfo::IntType, shards::CoreInfo::IntVarType});
  PARAM_PARAMVAR(_fps, "FPS", "Frames per second",
                 {shards::CoreInfo::NoneType, shards::CoreInfo::IntType, shards::CoreInfo::IntVarType,
                  shards::CoreInfo::FloatType, shards::CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_detached, "Detached",
                 "If true, the mesh will run on its own worker thread, and simply suspend on the parent context",
                 {shards::CoreInfo::BoolType});
  PARAM_IMPL(PARAM_IMPL_FOR(_mesh), PARAM_IMPL_FOR(_tickTime), PARAM_IMPL_FOR(_iterations), PARAM_IMPL_FOR(_fps),
             PARAM_IMPL_FOR(_detached));

  PARAM_REQUIRED_VARIABLES()
  SHTypeInfo compose(const SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);

    if (_mesh->valueType == SHType::None) {
      throw ComposeError("Schedule: Mesh parameter is required");
    }

    if (!_tickTime.isNone() && !_fps.isNone()) {
      throw std::runtime_error("Run: run requires either a TickTime or FPS parameter");
    }

    return data.inputType;
  }
  void warmup(SHContext *context) { PARAM_WARMUP(context); }
  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto &fpsVar = (Var &)_fps.get();
    auto &tickTimeVar = (Var &)_tickTime.get();
    auto &mesh = *reinterpret_cast<std::shared_ptr<SHMesh> *>(_mesh->payload.objectValue);
    auto &iterationsVar = (Var &)_iterations.get();
    bool detached = _detached->payload.boolValue;

    mesh->setParentContext(context);

    // Calculate target frame time based on fps or tickTime
    double targetFrameTime = 0.0;
    if (!tickTimeVar.isNone()) {
      targetFrameTime = tickTimeVar.payload.floatValue;
    } else if (!fpsVar.isNone()) {
      targetFrameTime =
          fpsVar.valueType == SHType::Int ? 1.0f / float(fpsVar.payload.intValue) : 1.0f / fpsVar.payload.floatValue;
    }

    // Calculate iterations limit
    size_t numIterations = ~0; // Indefinitely
    if (iterationsVar.valueType == SHType::Int) {
      numIterations = iterationsVar.payload.intValue;
    }

    if (detached) {
      // Run mesh on a worker thread
      std::atomic<bool> done{false};
      std::atomic<bool> noErrors{true};

      SHLOG_DEBUG("Running mesh on a detached worker thread");

      std::thread worker([&mesh, &done, &noErrors, targetFrameTime, numIterations]() {
        size_t iteration = 0;
        std::chrono::steady_clock::time_point frameStart;
        double frameTime = 0.0;
        double accumulator = 0.0;

        while (!mesh->empty()) {
          frameStart = std::chrono::steady_clock::now();

          // Process frame
          if (!mesh->tick()) {
            noErrors = false;
          }

          // Calculate actual time spent processing
          frameTime = std::chrono::duration<double>(std::chrono::steady_clock::now() - frameStart).count();

          // Sleep only if needed, compensating for processing time
          if (targetFrameTime > 0.0) {
            double sleepTime = targetFrameTime - frameTime;
            // Add accumulated error from previous frames
            sleepTime -= accumulator;

            if (sleepTime > 0.001) { // Only sleep for meaningful durations (>1ms)
              shards::sleep(sleepTime);
              // Calculate actual sleep time for error accumulation
              double actualSleepTime =
                  std::chrono::duration<double>(std::chrono::steady_clock::now() - frameStart).count() - frameTime;
              // Store sleep error for next frame compensation
              accumulator = actualSleepTime - sleepTime;
            } else {
              // If we're running behind, add to accumulator
              accumulator = -sleepTime;
              // Small yield to prevent CPU hogging
              std::this_thread::yield();
            }
          }

          if (mesh->empty())
            break;
          if (numIterations != size_t(~0) && ++iteration >= numIterations) {
            break;
          }
        }

        SHLOG_DEBUG("Mesh (detached) is done, terminating, without errors: {}", noErrors);

        // Terminate the mesh
        mesh->terminate();
        done = true;
      });

      worker.detach(); // Detach worker thread

      SHLOG_DEBUG("Detached worker thread started");

      // Suspend main thread until mesh is done
      while (!done) {
        SH_SUSPEND(context, 0); // Yield to parent mesh
      }

      return Var(noErrors.load());
    } else {
      // always use SH_SUSPEND on emscripten
#ifdef __EMSCRIPTEN__
      bool hasParentMesh = true;
#else
      bool hasParentMesh = mesh->parent != nullptr;
#endif

      size_t iteration = 0;
      bool noErrors = true;
      std::chrono::steady_clock::time_point frameStart;
      double frameTime = 0.0;
      double accumulator = 0.0;

      // Run mesh on main thread
      while (!mesh->empty()) {
        frameStart = std::chrono::steady_clock::now();

        // Process frame
        if (!mesh->tick()) {
          noErrors = false;
        }

        // Calculate actual time spent processing
        frameTime = std::chrono::duration<double>(std::chrono::steady_clock::now() - frameStart).count();

        // Sleep with compensation
        if (targetFrameTime > 0.0) {
          double sleepTime = targetFrameTime - frameTime;
          // Add accumulated error from previous frames
          sleepTime -= accumulator;

          if (sleepTime > 0.001) {
            if (hasParentMesh) {
              SH_SUSPEND(context, sleepTime);
              // Calculate actual sleep time for error accumulation
              double actualSleepTime =
                  std::chrono::duration<double>(std::chrono::steady_clock::now() - frameStart).count() - frameTime;
              // Store sleep error for next frame compensation
              accumulator = actualSleepTime - sleepTime;
            } else {
              shards::sleep(sleepTime);
              // Calculate actual sleep time for error accumulation
              double actualSleepTime =
                  std::chrono::duration<double>(std::chrono::steady_clock::now() - frameStart).count() - frameTime;
              // Store sleep error for next frame compensation
              accumulator = actualSleepTime - sleepTime;
            }
          } else {
            // If we're running behind, add to accumulator (capped)
            accumulator = std::min(-sleepTime, 0.1); // Cap drift compensation
            if (hasParentMesh) {
              SH_SUSPEND(context, 0); // Yield to parent mesh
            } else {
              std::this_thread::yield(); // Yield instead of sleeping
            }
          }
        }

        if (mesh->empty())
          break;
        if (numIterations != size_t(~0) && ++iteration >= numIterations) {
          break;
        }
      }

      // Terminate the mesh
      SPDLOG_TRACE("Mesh is done, terminating, without errors: {}", noErrors);
      mesh->terminate();

      return Var(noErrors);
    }
  }
};

} // namespace run_
} // namespace shards
SHARDS_REGISTER_FN(run) {
  using namespace shards::run_;
  REGISTER_SHARD("Schedule", Schedule);
  REGISTER_SHARD("Run", Run);
}
