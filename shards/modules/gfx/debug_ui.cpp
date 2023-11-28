#include "../inputs/inputs.hpp"
#include "gfx.hpp"
#include "debug_ui.hpp"
#include <optional>
#include <gfx/debug/debugger.hpp>
#include <gfx/debug/debugger_data.hpp>
#include <shards/core/params.hpp>
#include <shards/common_types.hpp>
#include <shards/core/module.hpp>

using namespace shards::input;
using ::gfx::GraphicsContext;
using ::gfx::RequiredGraphicsContext;

extern "C" {
void shards_gfx_freeString(const char *str) { free((void *)str); }
}

namespace debug_ui {
using namespace shards::gfx::debug_ui;
}

namespace shards::gfx::debug_ui {
struct DebugUI {
  RequiredInputContext _inputContext;
  RequiredGraphicsContext _graphicsContext;
  SHVar *_uiContext{};

  debug_ui::UIOpts _opts{};

  static inline Type ContextType = Type::VariableOf(GraphicsContext::Type);

  PARAM_PARAMVAR(_renderer, "Renderer", "The renderer to pull the debug information from", {ContextType})
  PARAM_IMPL(PARAM_IMPL_FOR(_renderer));

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString help() { return SHCCSTR("Shows the graphics system debug UI"); }

  static inline Type EguiUiType = Type::Object(CoreCC, 'eguU');
  static inline Type EguiContextType = Type::Object(CoreCC, 'eguC');
  static inline char EguiContextName[] = "UI.Contexts";

  static void mergeRequiredUITypes(ExposedInfo &out) {
    out.push_back(SHExposedTypeInfo{
        .name = EguiContextName,
        .exposedType = EguiContextType,
    });
  }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);

    _requiredVariables.push_back(_inputContext.getExposedTypeInfo());
    mergeRequiredUITypes(_requiredVariables);

    _graphicsContext.compose(data, _requiredVariables, &_renderer);

    return CoreInfo::NoneType;
  }

  void *_state{};

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _inputContext.warmup(context);
    _graphicsContext.warmup(context, &_renderer);

    if (!_state)
      _state = shards_gfx_newDebugUI();

    _uiContext = referenceVariable(context, EguiContextName);
  }

  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);
    _inputContext.cleanup();
    _graphicsContext.cleanup();

    if (_state) {
      shards_gfx_freeDebugUI(_state);
      _state = nullptr;
    }

    releaseVariable(_uiContext);
    _uiContext = nullptr;
  }

  std::vector<debug_ui::FrameQueue> frameQueues;
  std::vector<debug_ui::Node> nodes;
  std::vector<debug_ui::PipelineGroup> pipelineGroups;
  std::vector<debug_ui::Output> outputs;
  std::vector<debug_ui::FrameIndex> indices;
  std::vector<debug_ui::Frame> frames;
  std::vector<debug_ui::Target> targets;
  std::vector<debug_ui::Drawable> drawables;
  std::list<std::string> strings;

  const char *storeString(std::string str) {
    strings.emplace_back(std::move(str));
    return strings.back().c_str();
  }

  void reset() {
    frameQueues.clear();
    nodes.clear();
    pipelineGroups.clear();
    outputs.clear();
    indices.clear();
    frames.clear();
    targets.clear();
    drawables.clear();
    strings.clear();
  }

  struct Counters {
    size_t numOutputs = 0;
    size_t numNodes = 0;
    size_t numFrames = 0;
    size_t numIndices = 0;
    size_t numPipelineGroups = 0;
    size_t numDrawables = 0;
    size_t numTargets = 0;
  };

  template <typename T> static T *sliceBuffer(std::vector<T> &vec, size_t &outLen, size_t &inOffset, size_t inLength) {
    if (inLength == 0) {
      outLen = 0;
      return nullptr;
    }
    assert(vec.size() >= (inOffset + inLength));
    T *ptr = &vec[inOffset];
    inOffset += inLength;
    outLen = inLength;
    return ptr;
  }

  SHVar activate(SHContext *shContext, const SHVar &input) {
    auto &debugger = _graphicsContext->debugger;
    debug_ui::UIParams params{
        .state = _state,
        .opts = _opts,
        .debuggerEnabled = (bool)debugger,
    };

    reset();

    if (debugger) {
      auto log = debugger->_impl->getLog();
      params.currentFrame = log->frameCounter;

      {
        Counters c;
        for (auto &fq : log->frameQueues) {
          frameQueues.emplace_back();
          c.numFrames += fq.frames.size();
          c.numOutputs += fq.outputs.size();
          for (auto &node : fq.nodes) {
            c.numNodes++;
            for (auto &pg : node.pipelineGroups) {
              c.numPipelineGroups++;
              c.numDrawables += pg.drawables.size();
            }
            c.numIndices += node.readsFrom.size();
            c.numIndices += node.writesTo.size();
            c.numTargets += node.renderTargetLayout.targets.size();
          }
        }
        outputs.resize(c.numOutputs);
        nodes.resize(c.numNodes);
        indices.resize(c.numIndices);
        frames.resize(c.numFrames);
        pipelineGroups.resize(c.numPipelineGroups);
        targets.resize(c.numTargets);
        drawables.resize(c.numDrawables);
      }

      Counters c;
      for (size_t fqIdx = 0; fqIdx < log->frameQueues.size(); fqIdx++) {
        auto &fq = log->frameQueues[fqIdx];
        auto &outFq = frameQueues[fqIdx];
        outFq.nodes = sliceBuffer(nodes, outFq.numNodes, c.numNodes, fq.nodes.size());
        for (size_t nodeIdx = 0; nodeIdx < fq.nodes.size(); nodeIdx++) {
          auto &node = fq.nodes[nodeIdx];
          auto &outNode = outFq.nodes[nodeIdx];
          if (nodeIdx > 0 && !node.pipelineGroups.empty()) {
            SPDLOG_DEBUG("e");
          }

          outNode.targets = sliceBuffer(targets, outNode.numTargets, c.numTargets, node.renderTargetLayout.targets.size());
          outNode.pipelineGroups =
              sliceBuffer(pipelineGroups, outNode.numPipelineGroups, c.numPipelineGroups, node.pipelineGroups.size());
          for (size_t targetIdx = 0; targetIdx < node.renderTargetLayout.targets.size(); targetIdx++) {
            auto &target = node.renderTargetLayout.targets[targetIdx];
            outNode.targets[targetIdx].name = storeString(target.name);
          }
          for (size_t pgIdx = 0; pgIdx < node.pipelineGroups.size(); pgIdx++) {
            auto &pg = node.pipelineGroups[pgIdx];
            auto &outPg = outNode.pipelineGroups[pgIdx];

            outPg.drawables = sliceBuffer(drawables, outPg.numDrawables, c.numDrawables, pg.drawables.size());
            for (size_t drawableIdx = 0; drawableIdx < pg.drawables.size(); drawableIdx++) {
              auto &drawable = pg.drawables[drawableIdx];
              outPg.drawables[drawableIdx].id = uint64_t(drawable.value & ::gfx::UniqueIdIdMask);
            }
          }
        }

        outFq.frames = sliceBuffer(frames, outFq.numFrames, c.numFrames, fq.frames.size());
        for (size_t frameIdx = 0; frameIdx < fq.frames.size(); frameIdx++) {
          auto &frame = fq.frames[frameIdx];
          auto &outFrame = outFq.frames[frameIdx];
          outFrame.name = storeString(frame.name);
          outFrame.size = Int2{frame.size.x, frame.size.y};
          outFrame.format = int32_t(frame.format);
          outFrame.isOutput = frame.outputIndex.has_value();
        }
      }

      params.frameQueues = frameQueues.data();
      params.numFrameQueues = frameQueues.size();
    }

    shards_gfx_showDebugUI(_uiContext, params);
    return SHVar{};
  }
};
} // namespace shards::gfx::debug_ui

SHARDS_REGISTER_FN(debug) {
  using namespace shards::gfx::debug_ui;
  REGISTER_SHARD("GFX.DebugUI", DebugUI);
}
