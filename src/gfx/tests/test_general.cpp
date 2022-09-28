#include "./context.hpp"
#include "./data.hpp"
#include "./renderer.hpp"
#include "renderer_utils.hpp"
#include <catch2/catch_all.hpp>
#include <gfx/context.hpp>
#include <gfx/drawable.hpp>
#include <gfx/features/wireframe.hpp>
#include <gfx/features/velocity.hpp>
#include <gfx/gizmos/wireframe.hpp>
#include <gfx/loop.hpp>
#include <gfx/paths.hpp>
#include <gfx/texture.hpp>
#include <gfx/texture_file/texture_file.hpp>
#include <gfx/render_target.hpp>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>

using namespace gfx;

static constexpr float comparisonTolerance = 0.05f;

TEST_CASE("Power of 2", "[Math]") {
  CHECK(isPOT(1) == true);
  CHECK(isPOT(2) == true);
  CHECK(isPOT(3) == false);
  CHECK(isPOT(4) == true);
  CHECK(isPOT(5) == false);
  CHECK(isPOT(8) == true);

  CHECK(alignTo<2>(0) == 0);
  CHECK(alignTo<2>(1) == 2);
  CHECK(alignTo<2>(3) == 4);
  CHECK(alignTo<2>(4) == 4);
  CHECK(alignTo<4>(0) == 0);
  CHECK(alignTo<4>(1) == 4);
  CHECK(alignTo<4>(3) == 4);
  CHECK(alignTo<4>(4) == 4);
  CHECK(alignTo<4>(5) == 8);
  CHECK(alignTo<4>(8) == 8);
  CHECK(alignTo<4>(8) == 8);
  CHECK(alignTo<6>(5) == 6);
  CHECK(alignTo<6>(12) == 12);

  CHECK(alignTo<4>(0) == alignTo(0, 4));
  CHECK(alignTo<2>(3) == alignTo(3, 2));
  CHECK(alignTo<2>(30) == alignTo(30, 2));
  CHECK(alignTo<16>(3) == alignTo(3, 16));
  CHECK(alignTo<16>(30) == alignTo(30, 16));
  CHECK(alignTo<4>(4) == alignTo(4, 4));
}

TEST_CASE("Renderer capture", "[General]") {
  auto testRenderer = createTestRenderer();
  Renderer &renderer = *testRenderer->renderer.get();

  MeshPtr mesh = createSphereMesh();
  DrawablePtr drawable = std::make_shared<Drawable>(mesh);
  ViewPtr view = std::make_shared<View>();

  DrawQueuePtr queue = std::make_shared<DrawQueue>();
  queue->add(drawable);

  PipelineSteps steps = createTestPipelineSteps(queue);
  TEST_RENDER_LOOP(testRenderer) { renderer.render(view, steps); };
  CHECK(testRenderer->checkFrame("capture", comparisonTolerance));

  testRenderer.reset();
}

TEST_CASE("Multiple vertex formats", "[General]") {
  auto testRenderer = createTestRenderer();
  Renderer &renderer = *testRenderer->renderer.get();

  std::vector<MeshPtr> meshes;

  geom::SphereGenerator sphere;
  sphere.generate();
  meshes.push_back(createMesh(sphere.vertices, sphere.indices));
  meshes.push_back(createMesh(convertVertices<VertexP>(sphere.vertices), sphere.indices));
  auto coloredVertices = convertVertices<VertexPC>(sphere.vertices);
  for (auto &vert : coloredVertices)
    vert.setColor(float4(0, 1, 0, 1));
  meshes.push_back(createMesh(coloredVertices, sphere.indices));

  ViewPtr view = createTestProjectionView();

  DrawQueuePtr queue = std::make_shared<DrawQueue>();

  float offset = -2.0f;
  for (auto &mesh : meshes) {
    float4x4 transform = linalg::translation_matrix(float3(offset, 0.0f, 0.0f));
    DrawablePtr drawable = std::make_shared<Drawable>(mesh, transform);
    queue->add(drawable);
    offset += 2.0f;
  }

  PipelineSteps steps = createTestPipelineSteps(queue);
  TEST_RENDER_LOOP(testRenderer) { renderer.render(view, steps); };
  CHECK(testRenderer->checkFrame("vertexFormats", comparisonTolerance));

  testRenderer.reset();
}

TEST_CASE("Pipeline states", "[General]") {
  auto testRenderer = createTestRenderer(int2(512, 512));
  Renderer &renderer = *testRenderer->renderer.get();

  geom::SphereGenerator sphere;
  sphere.generate();

  auto redSphereVerts = convertVertices<VertexPC>(sphere.vertices);
  for (auto &vert : redSphereVerts)
    vert.setColor(float4(1, 0, 0, 0.5));

  auto greenSphereVerts = convertVertices<VertexPC>(sphere.vertices);
  for (auto &vert : greenSphereVerts)
    vert.setColor(float4(0, 1, 0, 0.5));

  MeshPtr redSphereMesh = createMesh(redSphereVerts, sphere.indices);
  MeshPtr greenSphereMesh = createMesh(greenSphereVerts, sphere.indices);

  ViewPtr view = std::make_shared<View>();
  view->proj = ViewOrthographicProjection{
      .size = 2.0f,
      .sizeType = OrthographicSizeType::Horizontal,
      .near = 0.0f,
      .far = 4.0f,
  };

  DrawQueuePtr queue = std::make_shared<DrawQueue>();

  float4x4 transform = linalg::translation_matrix(float3(-0.5f, 0.0f, -1.0f));
  DrawablePtr drawable = std::make_shared<Drawable>(redSphereMesh, transform);
  queue->add(drawable);

  transform = linalg::translation_matrix(float3(0.5f, 0.0f, -3.0f));
  drawable = std::make_shared<Drawable>(greenSphereMesh, transform);
  queue->add(drawable);

  auto testBlendState = [&](const char *name, const BlendState &state) {
    FeaturePtr blendFeature = std::make_shared<Feature>();
    blendFeature->state.set_depthWrite(false);
    blendFeature->state.set_blend(state);

    PipelineSteps steps{
        makePipelineStep(RenderDrawablesStep{
            .drawQueue = queue,
            .features =
                {
                    features::Transform::create(),
                    features::BaseColor::create(),
                    blendFeature,
                },
            .sortMode = SortMode::BackToFront,
        }),
    };
    TEST_RENDER_LOOP(testRenderer) { renderer.render(view, steps); };
    CHECK(testRenderer->checkFrame(name, comparisonTolerance));
  };

  testBlendState("blendAlphaPremul", BlendState{.color = BlendComponent::AlphaPremultiplied, .alpha = BlendComponent::Opaque});
  testBlendState("blendAlpha", BlendState{.color = BlendComponent::Alpha, .alpha = BlendComponent::Opaque});
  testBlendState("blendAdditive", BlendState{.color = BlendComponent::Additive, .alpha = BlendComponent::Opaque});

  testRenderer.reset();
}

TEST_CASE("Shader parameters", "[General]") {
  auto testRenderer = createTestRenderer();
  Renderer &renderer = *testRenderer->renderer.get();

  MeshPtr sphereMesh = createSphereMesh();

  ViewPtr view = std::make_shared<View>();
  view->view = linalg::lookat_matrix(float3(0, 10.0f, 10.0f), float3(0, 0, 0), float3(0, 1, 0));
  view->proj = ViewPerspectiveProjection{
      degToRad(45.0f),
      FovDirection::Horizontal,
  };

  DrawQueuePtr queue = std::make_shared<DrawQueue>();
  float4x4 transform;
  DrawablePtr drawable;

  transform = linalg::translation_matrix(float3(-2.0f, 0.0f, 0.0f));
  drawable = std::make_shared<Drawable>(sphereMesh, transform);
  queue->add(drawable);

  transform = linalg::translation_matrix(float3(0.0f, 0.0f, 0.0f));
  drawable = std::make_shared<Drawable>(sphereMesh, transform);
  drawable->parameters.set("baseColor", float4(1, 0, 0, 1));
  queue->add(drawable);

  auto material = std::make_shared<Material>();
  material->parameters.set("baseColor", float4(0, 1, 0, 1));

  transform = linalg::translation_matrix(float3(2.0f, 0.0f, 0.0f));
  drawable = std::make_shared<Drawable>(sphereMesh, transform);
  drawable->material = material;
  queue->add(drawable);

  PipelineSteps steps{
      makePipelineStep(RenderDrawablesStep{
          .drawQueue = queue,
          .features =
              {
                  features::Transform::create(),
                  features::BaseColor::create(),
              },
      }),
  };

  TEST_RENDER_LOOP(testRenderer) { renderer.render(view, steps); };
  CHECK(testRenderer->checkFrame("shaderParameters", comparisonTolerance));

  testRenderer.reset();
}

TEST_CASE("Reference tracking", "[General]") {
  auto testRenderer = createTestRenderer();
  Renderer &renderer = *testRenderer->renderer.get();
  Context &context = *testRenderer->context.get();

  std::weak_ptr<Mesh> meshWeak;
  std::weak_ptr<Drawable> drawableWeak;
  std::weak_ptr<MeshContextData> meshContextData;
  {
    MeshPtr mesh = createSphereMesh();
    meshWeak = std::weak_ptr(mesh);

    mesh->createContextDataConditional(context);
    meshContextData = mesh->contextData;

    DrawablePtr drawable = std::make_shared<Drawable>(mesh);
    drawableWeak = std::weak_ptr(drawable);

    ViewPtr view = std::make_shared<View>();

    DrawQueuePtr queue = std::make_shared<DrawQueue>();
    queue->add(drawable);
    context.beginFrame();
    renderer.beginFrame();
    renderer.render(view, createTestPipelineSteps(queue));
    renderer.endFrame();
    context.endFrame();
  }

  CHECK(meshWeak.expired());
  CHECK(drawableWeak.expired());
  CHECK(!meshContextData.expired());

  context.sync();
  for (size_t i = 0; i < 2; i++) {
    context.beginFrame();
    renderer.beginFrame();
    renderer.endFrame();
    context.endFrame();
  }
  context.sync();

  // Should be released now
  CHECK(meshContextData.expired());

  testRenderer.reset();
}

TEST_CASE("Textures", "[General]") {
  auto testRenderer = createTestRenderer();
  Renderer &renderer = *testRenderer->renderer.get();

  MeshPtr planeMesh = createPlaneMesh();

  ViewPtr view = std::make_shared<View>();
  view->proj = ViewOrthographicProjection{
      .size = 2.0f,
      .near = -10.0f,
      .far = 10.0f,
  };

  DrawQueuePtr queue = std::make_shared<DrawQueue>();
  float4x4 transform;
  DrawablePtr drawable;

  // Texture with default sampler state (linear filtering)
  TexturePtr textureA = textureFromFile(resolveDataPath("src/gfx/tests/assets/pixely_sprite.png").string().c_str());

  // Texture with modified sampler (point filtering)
  TexturePtr textureB = textureA->clone();
  textureB->initWithSamplerState(SamplerState{
      .filterMode = WGPUFilterMode_Nearest,
  });

  transform = linalg::translation_matrix(float3(-.5f, 0.0f, 0.0f));
  drawable = std::make_shared<Drawable>(planeMesh, transform);
  drawable->parameters.set("baseColor", textureA);
  queue->add(drawable);

  transform = linalg::translation_matrix(float3(.5f, 0.0f, 0.0f));
  drawable = std::make_shared<Drawable>(planeMesh, transform);
  drawable->parameters.set("baseColor", textureB);
  queue->add(drawable);

  FeaturePtr blendFeature = std::make_shared<Feature>();
  blendFeature->state.set_blend(BlendState{
      .color = BlendComponent::Alpha,
      .alpha = BlendComponent::Opaque,
  });

  PipelineSteps steps{
      makePipelineStep(RenderDrawablesStep{
          .drawQueue = queue,
          .features =
              {
                  features::Transform::create(),
                  features::BaseColor::create(),
                  blendFeature,
              },
      }),
  };

  TEST_RENDER_LOOP(testRenderer) { renderer.render(view, steps); };
  CHECK(testRenderer->checkFrame("textures", comparisonTolerance));

  testRenderer.reset();
}

TEST_CASE("RenderTarget", "[General]") {
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

TEST_CASE("Velocity", "[General]") {
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
