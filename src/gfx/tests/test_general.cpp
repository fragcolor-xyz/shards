#include "./context.hpp"
#include "./data.hpp"
#include "./renderer.hpp"
#include "renderer_utils.hpp"
#include <catch2/catch_all.hpp>
#include <gfx/context.hpp>
#include <gfx/drawable.hpp>
#include <gfx/paths.hpp>
#include <gfx/texture.hpp>
#include <gfx/texture_file/texture_file.hpp>
#include <spdlog/fmt/fmt.h>


using namespace gfx;

static constexpr float comparisonTolerance = 0.05f;

TEST_CASE("Renderer capture", "[General]") {
  auto testRenderer = createTestRenderer();
  Renderer &renderer = *testRenderer->renderer.get();

  MeshPtr mesh = createSphereMesh();
  DrawablePtr drawable = std::make_shared<Drawable>(mesh);
  ViewPtr view = std::make_shared<View>();

  DrawQueue queue;
  queue.add(drawable);

  TEST_RENDER_LOOP(testRenderer) { renderer.render(queue, view, createTestPipelineSteps()); };
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

  DrawQueue queue;

  float offset = -2.0f;
  for (auto &mesh : meshes) {
    float4x4 transform = linalg::translation_matrix(float3(offset, 0.0f, 0.0f));
    DrawablePtr drawable = std::make_shared<Drawable>(mesh, transform);
    queue.add(drawable);
    offset += 2.0f;
  }

  TEST_RENDER_LOOP(testRenderer) { renderer.render(queue, view, createTestPipelineSteps()); };
  CHECK(testRenderer->checkFrame("vertexFormats", comparisonTolerance));

  testRenderer.reset();
}

TEST_CASE("Pipeline states", "[General][!mayfail]") {
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

  DrawQueue queue;

  float4x4 transform = linalg::translation_matrix(float3(-0.5f, 0.0f, -1.0f));
  DrawablePtr drawable = std::make_shared<Drawable>(redSphereMesh, transform);
  queue.add(drawable);

  transform = linalg::translation_matrix(float3(0.5f, 0.0f, -3.0f));
  drawable = std::make_shared<Drawable>(greenSphereMesh, transform);
  queue.add(drawable);

  auto testBlendState = [&](const char *name, const BlendState &state) {
    FeaturePtr blendFeature = std::make_shared<Feature>();
    blendFeature->state.set_depthWrite(false);
    blendFeature->state.set_blend(state);

    PipelineSteps steps{
        makeDrawablePipelineStep(RenderDrawablesStep{
            .features =
                {
                    features::Transform::create(),
                    features::BaseColor::create(),
                    blendFeature,
                },
            .sortMode = SortMode::BackToFront,
        }),
    };
    TEST_RENDER_LOOP(testRenderer) { renderer.render(queue, view, steps); };
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

  DrawQueue queue;
  float4x4 transform;
  DrawablePtr drawable;

  transform = linalg::translation_matrix(float3(-2.0f, 0.0f, 0.0f));
  drawable = std::make_shared<Drawable>(sphereMesh, transform);
  queue.add(drawable);

  transform = linalg::translation_matrix(float3(0.0f, 0.0f, 0.0f));
  drawable = std::make_shared<Drawable>(sphereMesh, transform);
  drawable->parameters.set("baseColor", float4(1, 0, 0, 1));
  queue.add(drawable);

  auto material = std::make_shared<Material>();
  material->parameters.set("baseColor", float4(0, 1, 0, 1));

  transform = linalg::translation_matrix(float3(2.0f, 0.0f, 0.0f));
  drawable = std::make_shared<Drawable>(sphereMesh, transform);
  drawable->material = material;
  queue.add(drawable);

  PipelineSteps steps{
      makeDrawablePipelineStep(RenderDrawablesStep{
          .features =
              {
                  features::Transform::create(),
                  features::BaseColor::create(),
              },
      }),
  };

  TEST_RENDER_LOOP(testRenderer) { renderer.render(queue, view, steps); };
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

    DrawQueue queue;
    queue.add(drawable);
    context.beginFrame();
    renderer.beginFrame();
    renderer.render(queue, view, createTestPipelineSteps());
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

  DrawQueue queue;
  float4x4 transform;
  DrawablePtr drawable;

  // Texture with default sampler state (linear filtering)
  TexturePtr textureA = textureFromFile(resolveDataPath("src/gfx/tests/assets/pixely_sprite.png").string().c_str());

  // Texture with modified sampler (point filtering)
  TexturePtr textureB = textureA->clone();
  textureB->setSamplerState(SamplerState{
      .filterMode = WGPUFilterMode_Nearest,
  });

  transform = linalg::translation_matrix(float3(-.5f, 0.0f, 0.0f));
  drawable = std::make_shared<Drawable>(planeMesh, transform);
  drawable->parameters.set("baseColor", textureA);
  queue.add(drawable);

  transform = linalg::translation_matrix(float3(.5f, 0.0f, 0.0f));
  drawable = std::make_shared<Drawable>(planeMesh, transform);
  drawable->parameters.set("baseColor", textureB);
  queue.add(drawable);

  FeaturePtr blendFeature = std::make_shared<Feature>();
  blendFeature->state.set_blend(BlendState{
      .color = BlendComponent::Alpha,
      .alpha = BlendComponent::Opaque,
  });

  PipelineSteps steps{
      makeDrawablePipelineStep(RenderDrawablesStep{
          .features =
              {
                  features::Transform::create(),
                  features::BaseColor::create(),
                  blendFeature,
              },
      }),
  };

  TEST_RENDER_LOOP(testRenderer) { renderer.render(queue, view, steps); };
  CHECK(testRenderer->checkFrame("textures", comparisonTolerance));

  testRenderer.reset();
}
