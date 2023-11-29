#ifndef DFF4306E_B56B_47D6_BB34_8EE7A39DF8B3
#define DFF4306E_B56B_47D6_BB34_8EE7A39DF8B3

#include "debugger.hpp"
#include "../drawable.hpp"
#include "../renderer_types.hpp"
#include "../renderer_storage.hpp"
#include "../shader/blocks.hpp"
#include "../frame_queue.hpp"
#include "../view.hpp"

namespace gfx {
struct PooledCopyTextureStep {
  PipelineStepPtr step;
};
namespace detail {
template <> struct SizedItemOps<PooledCopyTextureStep> {
  size_t getCapacity(PooledCopyTextureStep &item) const { return 0; }
  void init(PooledCopyTextureStep &item, size_t size) {}
};
template <> struct SizedItemOps<TexturePtr> {
  SizedItemOps(){};
  size_t getCapacity(TexturePtr &item) const { return 0; }
  void init(TexturePtr &item, size_t size) { item = std::make_shared<Texture>(); }
};
} // namespace detail

inline PipelineStepPtr copyTextureHelper(detail::SizedItemPool<PooledCopyTextureStep> &pool, gfx::TexturePtr inputTexture,
                                         gfx::TexturePtr outTexture) {
  using namespace shader::blocks;

  int2 inputSize = inputTexture->getResolution();
  int2 outputSize = outTexture->getResolution();

  // Basic best-fit placement into a square
  // float2 mult{};
  // float2 offset{};
  // if (inputSize.x > inputSize.y) {
  //   float aspect = float(inputSize.y) / float(inputSize.x);
  //   mult = float2(1.0f, aspect);
  //   offset.y = (1.0f - aspect) / 2.0;
  // } else {
  //   float aspect = float(inputSize.x) / float(inputSize.y);
  //   mult = float2(aspect, 1.0f);
  //   offset.x = (1.0f - aspect) / 2.0;
  // }

  // Best-fit placement into target rectangle
  float2 mult{};
  float2 offset{};
  float inputAspect = float(inputSize.x) / float(inputSize.y);
  float outputAspect = float(outputSize.x) / float(outputSize.y);
  if (inputAspect > outputAspect) {
    // Source image is wider than target
    float aspectRatioFactorX = outputAspect / inputAspect;
    mult = float2(1.0f, aspectRatioFactorX);
    offset.y = (1.0f - aspectRatioFactorX) / 2.0;
  } else {
    // Source image is taller than or equal to target
    float aspectRatioFactorY = inputAspect / outputAspect;
    mult = float2(aspectRatioFactorY, 1.0f);
    offset.x = (1.0f - aspectRatioFactorY) / 2.0;
  }

  static auto feature = []() {
    FeaturePtr feature = std::make_shared<Feature>();
    auto shader = makeCompoundBlock();
    shader->appendLine("let inUV = ", ReadInput("texCoord0"));
    shader->appendLine("let mult = ", ReadBuffer("mult", shader::FieldTypes::Float2));
    shader->appendLine("let offset = ", ReadBuffer("offset", shader::FieldTypes::Float2));
    shader->appendLine("let uv = inUV * mult + offset");
    shader->appendLine("let src = ", SampleTexture("copyInput", "uv"));
    shader->appendLine("if(uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) { discard; }");
    shader->appendLine(WriteOutput("copyOutput", shader::FieldTypes::Float4, "src"));
    feature->shaderEntryPoints.emplace_back("copy", ProgrammableGraphicsStage::Fragment, std::move(shader));
    return feature;
  }();

  auto &pooled = pool.allocateBuffer(0);
  if (!pooled.step) {
    RenderFullscreenStep s{
        .features = {feature},
    };
    pooled.step = makePipelineStep(s);
  }

  auto &s = std::get<RenderFullscreenStep>(*pooled.step.get());
  s.parameters = MaterialParameters{.basic = {
                                        {"mult", mult},
                                        {"offset", offset},
                                    }};
  s.input = RenderStepInput{
      .attachments =
          {
              RenderStepInput::Texture{
                  .name = "copyInput",
                  .subResource = TextureSubResource(inputTexture),
              },
          },
  };
  s.output = RenderStepOutput{
      .attachments =
          {
              RenderStepOutput::Texture{
                  .name = "copyOutput",
                  .subResource = TextureSubResource(outTexture),
                  .clearValues = ClearValues::getDefaultColor(),
              },
          },
      .sizeScale = std::nullopt,
  };

  return pooled.step;
}

namespace debug {
struct Node {
  using Attachment = detail::RenderGraphNode::Attachment;

  detail::RenderTargetLayout renderTargetLayout;
  std::vector<Attachment> writesTo;
  std::vector<TexturePtr> previewTextures;
  std::vector<detail::FrameIndex> readsFrom;

  // This points to which data slot to use when resolving view data
  size_t queueDataIndex;

  std::vector<PipelineGroupDesc> pipelineGroups;
};

struct FrameQueue {
  using Frame = detail::RenderGraph::Frame;
  using Output = detail::RenderGraph::Output;
  std::vector<Frame> frames;
  std::vector<Output> outputs;
  std::vector<Node> nodes;
};

struct Log {
  size_t frameCounter = 0;
  std::vector<FrameQueue> frameQueues;
  std::unordered_map<UniqueId, DrawablePtr> drawables;
};

struct DebuggerImpl {
  std::shared_mutex mutex;
  std::shared_ptr<Log> log;
  std::shared_ptr<Log> writeLog;
  size_t currentWriteNode;

  detail::Swappable<detail::SizedItemPool<TexturePtr>, 4> texturePools;
  size_t texturePoolIndex;

  detail::SizedItemPool<PooledCopyTextureStep> textureCopyPool;

  std::shared_ptr<Log> getLog() {
    std::shared_lock<std::shared_mutex> l(mutex);
    return log;
  }

  TexturePtr getDebugTexture(int2 referenceSize) {
    auto texture = texturePools(texturePoolIndex).allocateBuffer(0);
    TextureDesc desc{
        .format =
            TextureFormat{
                .flags = TextureFormatFlags::RenderAttachment,
                .pixelFormat = WGPUTextureFormat_RGBA8UnormSrgb,
            },
        .resolution = int2(256, 256),
    };
    texture->init(desc);
    return texture;
  }

  void beginFrame(detail::RendererStorage &storage) {
    assert(!writeLog);
    writeLog = std::make_shared<Log>();
    writeLog->frameCounter = storage.frameCounter;
    texturePoolIndex = gfx::mod<int32_t>(texturePoolIndex + 1, decltype(texturePools)::MaxSize);
    texturePools(texturePoolIndex).reset();
  }

  void endFrame() {
    assert(writeLog);

    // Finalize & swap
    std::unique_lock<std::shared_mutex> l(mutex);
    std::swap(log, writeLog);
    writeLog.reset();
  }

  void debugRender(PipelineSteps steps, detail::RendererStorage &storage, Renderer &renderer) {
    detail::FrameQueue tempFq(nullptr, storage, storage.workerMemory);
    static ViewPtr NoView = std::make_shared<View>();
    static detail::CachedView CachedIdentityView = []() {
      detail::CachedView v;
      v.touchWithNewTransform(NoView->view, NoView->getProjectionMatrix(float2(1.0f)), 1);
      return v;
    }();

    detail::ViewData vd{
        .view = NoView,
        .cachedView = CachedIdentityView,
    };

    tempFq.enqueue(vd, steps);
    tempFq.evaluate(renderer, true);
  }

  void copyNodeTextures(detail::RenderGraphEvaluator &evaluator) {
    auto &fq = writeLog->frameQueues.back();
    auto &node = fq.nodes[currentWriteNode];
    node.previewTextures.resize(node.writesTo.size());

    textureCopyPool.reset();

    PipelineSteps debugSteps;
    for (auto &idx : node.writesTo) {
      TexturePtr texture = evaluator.getTexture(idx.frameIndex);
      if (texture->getResolution() == int2(0, 0)) {
        continue;
      }
      if (!textureFormatFlagsContains(texture->getFormat().flags, TextureFormatFlags::NoTextureBinding)) {
        TexturePtr previewTexture = getDebugTexture(texture->getResolution());
        debugSteps.emplace_back(copyTextureHelper(textureCopyPool, texture, previewTexture));
      }
    }

    debugRender(debugSteps, evaluator.getStorage(), evaluator.getRenderer());
  }
};
} // namespace debug
} // namespace gfx

#endif /* DFF4306E_B56B_47D6_BB34_8EE7A39DF8B3 */
