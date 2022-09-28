#include "./context.hpp"
#include "./data.hpp"
#include "./renderer.hpp"
#include "renderer_utils.hpp"
#include <catch2/catch_all.hpp>
#include <gfx/fwd.hpp>
#include <gfx/context.hpp>
#include <gfx/drawable.hpp>
#include <gfx/features/velocity.hpp>
#include <gfx/loop.hpp>
#include <gfx/paths.hpp>
#include <gfx/texture.hpp>
#include <gfx/render_target.hpp>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

using namespace gfx;

static constexpr float comparisonTolerance = 0.05f;

TEST_CASE("RenderTarget", "[RenderGraph]") {
  auto testRenderer = createTestRenderer();
  Renderer &renderer = *testRenderer->renderer.get();
  auto &viewStack = renderer.getViewStack();

  MeshPtr cubeMesh = createCubeMesh();

  ViewPtr view = std::make_shared<View>();
  view->proj = ViewPerspectiveProjection{};
  view->view = linalg::lookat_matrix(float3(1.0f, 1.0f, 1.0f) * 3.0f, float3(0, 0, 0), float3(0, 1, 0));

  ViewPtr subView = std::make_shared<View>();

  DrawQueuePtr queue0 = std::make_shared<DrawQueue>();
  float4x4 transform;
  DrawablePtr drawable;

  std::shared_ptr<RenderTarget> rt = std::make_shared<RenderTarget>("testTarget");
  rt->configure("color", WGPUTextureFormat::WGPUTextureFormat_RGBA8Unorm);
  rt->configure("depth", WGPUTextureFormat::WGPUTextureFormat_Depth32Float);

  transform = linalg::identity;
  drawable = std::make_shared<Drawable>(cubeMesh, transform);
  drawable->parameters.set("baseColor", float4(1, 0, 1, 1));
  queue0->add(drawable);

  FeaturePtr blendFeature = std::make_shared<Feature>();
  blendFeature->state.set_blend(BlendState{
      .color = BlendComponent::Alpha,
      .alpha = BlendComponent::Opaque,
  });

  PipelineSteps stepsRT{
      makePipelineStep(RenderDrawablesStep{
          .drawQueue = queue0,
          .features =
              {
                  features::Transform::create(),
                  features::BaseColor::create(),
                  blendFeature,
              },
          .renderTarget = rt,
      }),
  };

  PipelineSteps stepsMain{
      makePipelineStep(ClearStep{
          .clearValues{
              .color = float4(0.2, 0.2, 0.2, 1.0),
          },
      }),
      makePipelineStep(RenderFullscreenStep{
          .features =
              {
                  features::Transform::create(false, false),
                  []() {
                    auto baseColor = features::BaseColor::create();
                    baseColor->state.clear_blend();
                    return baseColor;
                  }(),
              },
          .parameters{
              .texture = {{"baseColor", TextureParameter(rt->getAttachment("color"))}},
          },
      }),
  };

  TEST_RENDER_LOOP(testRenderer) {
    auto outputSize = testRenderer->getOutputSize();
    rt->resizeConditional(outputSize);

    Rect mainViewport = viewStack.getOutput().viewport;
    Rect subViewport = Rect(int2(mainViewport.width / 2, mainViewport.x), mainViewport.getSize() / 2);

    viewStack.push(ViewStack::Item{.renderTarget = rt});
    renderer.render(view, stepsRT);
    viewStack.pop();

    viewStack.push(ViewStack::Item{.viewport = subViewport});
    renderer.render(subView, stepsMain);
    viewStack.pop();
  };
  CHECK(testRenderer->checkFrame("renderTarget", comparisonTolerance));

  testRenderer.reset();
}

TEST_CASE("Velocity", "[RenderGraph]") {
  auto testRenderer = createTestRenderer();
  Renderer &renderer = *testRenderer->renderer.get();
  auto &viewStack = renderer.getViewStack();

  MeshPtr cubeMesh = createCubeMesh();

  ViewPtr view = std::make_shared<View>();
  view->proj = ViewPerspectiveProjection{};
  view->view = linalg::lookat_matrix(float3(1.0f, 1.0f, 1.0f) * 3.0f, float3(0, 0, 0), float3(0, 1, 0));

  DrawQueuePtr queue = std::make_shared<DrawQueue>();
  float4x4 transform;
  DrawablePtr drawable;

  std::shared_ptr<RenderTarget> rt = std::make_shared<RenderTarget>("testTarget");
  rt->configure("color", WGPUTextureFormat::WGPUTextureFormat_RGBA8Unorm);
  rt->configure("depth", WGPUTextureFormat::WGPUTextureFormat_Depth32Float);
  rt->configure("velocity", WGPUTextureFormat::WGPUTextureFormat_RG8Snorm);

  transform = linalg::identity;
  drawable = std::make_shared<Drawable>(cubeMesh, transform);

  auto getTransform = [](double time) {
    float yWave = std::cos(time / pi2) * 0.5f;
    float4x4 transMat = linalg::translation_matrix(float3(0, yWave, 0));

    float angle = degToRad(time * 15.0f + std::cos(time / pi2 * 3.0f) * 20.0f);
    float4 rotQY = linalg::rotation_quat(float3(0, 1, 0), angle);
    float4x4 rotMat = linalg::rotation_matrix(rotQY);
    return linalg::mul(transMat, rotMat);
  };

  queue->add(drawable);

  auto getWaveTransform = [](double time, size_t axis) {
    float3 t{};
    t[axis] = std::cos(time / pi2 * 20.0f) * 1.0f;
    return linalg::mul(linalg::translation_matrix(t), linalg::scaling_matrix(float3(0.25f)));
  };

  DrawablePtr drawable1 = std::make_shared<Drawable>(cubeMesh, getWaveTransform(0.0f, 0));
  queue->add(drawable1);

  DrawablePtr drawable2 = std::make_shared<Drawable>(cubeMesh, getWaveTransform(0.0f, 1));
  queue->add(drawable2);

  PipelineSteps stepsRT{
      makePipelineStep(RenderDrawablesStep{
          .drawQueue = queue,
          .features =
              {
                  features::Transform::create(),
                  features::Velocity::create(),
              },
          .renderTarget = rt,
      }),
  };

  auto postFeature = []() {
    using namespace shader;
    using namespace shader::blocks;

    auto feature = std::make_shared<Feature>();
    auto code = makeCompoundBlock();
    code->appendLine("let vel = ", SampleTexture("velocity"), ".xy");
    code->appendLine(WriteOutput("color", FieldTypes::Float4, "vec4<f32>(vel.x*0.5+0.5, vel.y*0.5+0.5, 0.0, 1.0)"));
    feature->shaderEntryPoints.emplace_back("", ProgrammableGraphicsStage::Fragment, std::move(code));
    feature->textureParams.emplace_back("velocity");
    return feature;
  }();

  PipelineSteps stepsMain{
      makePipelineStep(RenderFullscreenStep{
          .features = {features::Transform::create(false, false), postFeature},
          .parameters{
              .texture = {{"velocity", TextureParameter(rt->getAttachment("velocity"))}},
          },
      }),
  };

  double t = 3.0;
  double timeStep = 1.0 / 60.0;

  auto renderFrame = [&]() {
    auto outputSize = testRenderer->getOutputSize();
    rt->resizeConditional(outputSize);

    Rect mainViewport = viewStack.getOutput().viewport;
    Rect subViewport = Rect(int2(mainViewport.width / 2, mainViewport.x), mainViewport.getSize() / 2);

    drawable->transform = getTransform(t);
    drawable1->transform = getWaveTransform(t, 0);
    drawable2->transform = getWaveTransform(t, 1);

    viewStack.push(ViewStack::Item{.renderTarget = rt});
    renderer.render(view, stepsRT);
    viewStack.pop();

    renderer.render(view, stepsMain);

    t += timeStep;
  };

  // dummy frame to intialize previous transforms
  testRenderer->begin();
  renderFrame();
  testRenderer->end();

  TEST_RENDER_LOOP(testRenderer) { renderFrame(); };
  CHECK(testRenderer->checkFrame("velocity", comparisonTolerance));

  testRenderer.reset();
}
