#include "velocity.hpp"
#include "drawables/mesh_drawable.hpp"
#include "linalg.h"
#include "unique_id.hpp"
#include <gfx/enums.hpp>
#include <gfx/feature.hpp>
#include <gfx/params.hpp>
#include <gfx/shader/blocks.hpp>
#include <gfx/pipeline_builder.hpp>
#include <gfx/renderer_types.hpp>
#include <gfx/renderer_cache.hpp>
#include <gfx/drawables/mesh_drawable.hpp>
#include <memory>
#include <unordered_map>

namespace gfx::features {

struct DrawableVelocityCache {
  struct Entry {
    float4x4 previousTransform;
    float4x4 currentTransform;
    size_t lastTouched{};
    void touch(const float4x4 &transform, size_t frameCounter) {
      if (frameCounter > lastTouched) {
        previousTransform = currentTransform;
        currentTransform = transform;

        lastTouched = frameCounter;
      }
    }
  };

  void touch(size_t frameCounter) {
    if (lastTouched != frameCounter) {
      detail::clearOldCacheItemsIn(cache, frameCounter, 120 * 60);
      lastTouched = frameCounter;
    }
  }

  size_t lastTouched{};
  std::unordered_map<UniqueId, Entry> cache;
};

FeaturePtr Velocity::create(bool applyView, bool applyProjection) {
  using namespace shader;
  using namespace shader::blocks;

  struct PipelineModifier : public IPipelineModifier {
    void buildPipeline(PipelineBuilder &builder, const BuildPipelineOptions& options) {
      auto &objectBinding = builder.getOrCreateBufferBinding("object");
      objectBinding.bindGroupId = BindGroupId::Draw;
      objectBinding.structType->addField("previousWorld", Types::Float4x4);

      auto &viewBinding = builder.getOrCreateBufferBinding("view");
      viewBinding.bindGroupId = BindGroupId::View;
      viewBinding.structType->addField("previousView", Types::Float4x4);
    }
  };

  auto cache = std::make_shared<DrawableVelocityCache>();

  FeaturePtr feature = std::make_shared<Feature>();
  feature->pipelineModifier = std::make_shared<PipelineModifier>();
  feature->generators.emplace_back([cache](FeatureDrawableGeneratorContext &ctx) {
    for (size_t i = 0; i < ctx.getSize(); i++) {
      auto &drawable = ctx.getDrawable(i);

      // Update cached data
      if (const MeshDrawable *md = dynamic_cast<const MeshDrawable *>(&drawable)) {
        auto &entry = detail::getCacheEntry(cache->cache, drawable.getId());
        entry.touch(md->transform, ctx.frameCounter);
        if (md->previousTransform) {
          entry.previousTransform = md->previousTransform.value();
        }

        ctx.getParameterCollector(i).setParam("previousWorld", entry.previousTransform);
      }
    }
    cache->touch(ctx.frameCounter);
  });

  feature->generators.emplace_back([](FeatureViewGeneratorContext &ctx) {
    ctx.getParameterCollector().setParam("previousView", ctx.cachedView.previousViewTransform);
  });

  {
    auto code = blocks::makeCompoundBlock();
    code->appendLine("let localPosition = ", ReadGlobal("localPosition"));

    code->append("let prevScreenPosition = ");
    if (applyProjection)
      code->append(ReadBuffer("proj", Types::Float4x4, "view"), "*");
    if (applyView)
      code->append(ReadBuffer("previousView", Types::Float4x4, "view"), "*");
    code->append(ReadBuffer("previousWorld", Types::Float4x4, "object"), "*");
    code->appendLine("localPosition");
    code->appendLine(WriteOutput("previousPosition", Types::Float4, "prevScreenPosition"));

    auto &writePreviousPosition =
        feature->shaderEntryPoints.emplace_back("writePreviousPosition", ProgrammableGraphicsStage::Vertex, std::move(code));
    writePreviousPosition.dependencies.emplace_back("initWorldPosition");
  }

  {
    auto code = blocks::makeCompoundBlock();
    // NOTE: Fragment shader position input is in framebuffer space
    // https://www.w3.org/TR/WGSL/#builtin-values
    code->appendLine("let screenPosition = ", ReadInput("position"));
    code->appendLine("let viewport = ", ReadBuffer("viewport", Types::Float4, "view"));
    code->appendLine("let ndc = (screenPosition.xy - viewport.xy) / viewport.zw * vec2<f32>(2.0, -2.0) - vec2<f32>(1.0, -1.0)");

    code->appendLine("let prevNdc = ", ReadInput("previousPosition"));
    code->appendLine("let ndcVelocity = (ndc.xy - prevNdc.xy/prevNdc.w)");
    code->appendLine(WriteGlobal("velocity", Types::Float2, "ndcVelocity"));

    // Apply scale to fit in output precision
    code->appendLine(WithOutput("velocity", WriteOutput("velocity", Types::Float2, ReadGlobal("velocity"),
                                                        fmt::format(" * {:0.2}", scalingFactor))));

    feature->shaderEntryPoints.emplace_back("initVelocity", ProgrammableGraphicsStage::Fragment, std::move(code));
  }

  return feature;
}
} // namespace gfx::features
