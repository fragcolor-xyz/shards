#include "./context.hpp"
#include "./data.hpp"
#include "./renderer.hpp"
#include "renderer_utils.hpp"
#include <catch2/catch_all.hpp>
#include <gfx/context.hpp>
#include <gfx/drawable.hpp>
#include <gfx/features/wireframe.hpp>
#include <gfx/features/velocity.hpp>
#include <gfx/drawables/mesh_drawable.hpp>
#include <gfx/gizmos/wireframe.hpp>
#include <gfx/loop.hpp>
#include <gfx/paths.hpp>
#include <gfx/texture.hpp>
#include <gfx/texture_file/texture_file.hpp>
#include <gfx/render_target.hpp>
#include <spdlog/fmt/fmt.h>
#include <spdlog/spdlog.h>
#include <type_traits>

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
  DrawablePtr drawable = std::make_shared<MeshDrawable>(mesh);
  ViewPtr view = std::make_shared<View>();

  DrawQueuePtr queue = std::make_shared<DrawQueue>();
  queue->add(drawable);

  PipelineSteps steps = createTestPipelineSteps(queue);
  TEST_RENDER_LOOP(testRenderer) { renderer.render(view, steps); };
  CHECK(testRenderer->checkFrame("capture", comparisonTolerance));

  testRenderer.reset();
}

template <StorageType ST, typename T, typename T2 = void> struct TestVertexFormat {};

template <StorageType ST, typename T>
struct TestVertexFormat<ST, T, typename std::enable_if<!std::is_floating_point_v<T>>::type> {
  using ValueType = T;
  StorageType storageType = ST;
  ValueType value;
  ValueType max = std::numeric_limits<T>::max();
  ValueType zero = 0;
  TestVertexFormat(ValueType value = std::numeric_limits<T>::max() / 2) : value(value) {}
};

template <StorageType ST, typename T> struct TestVertexFormat<ST, T, typename std::enable_if<std::is_floating_point_v<T>>::type> {
  using ValueType = T;
  StorageType storageType = ST;
  ValueType value;
  ValueType max = 1.0;
  ValueType zero = 0.0;
  TestVertexFormat(ValueType value = 0.5) : value(value) {}
};

struct TestVertexFormatHalf {
  using ValueType = uint16_t;
  StorageType storageType = StorageType::Float16;
  // == 0.5
  // https://evanw.github.io/float-toy/
  ValueType value = 0x3800;
  ValueType max = 0x3C00;
  ValueType zero = 0;
};

template <typename T> void visitTestVertexFormats(T visitor) {
  visitor(TestVertexFormat<StorageType::UInt8, uint8_t>());
  visitor(TestVertexFormat<StorageType::Int8, int8_t>());
  visitor(TestVertexFormat<StorageType::UNorm8, uint8_t>());
  visitor(TestVertexFormat<StorageType::SNorm8, int8_t>());
  visitor(TestVertexFormat<StorageType::UInt16, uint16_t>());
  visitor(TestVertexFormat<StorageType::Int16, int16_t>());
  visitor(TestVertexFormat<StorageType::UNorm16, uint16_t>());
  visitor(TestVertexFormat<StorageType::SNorm16, int16_t>());
  visitor(TestVertexFormat<StorageType::UInt32, uint32_t>());
  visitor(TestVertexFormat<StorageType::Int32, int32_t>());
  visitor(TestVertexFormatHalf());
  visitor(TestVertexFormat<StorageType::Float32, float>());
}

TEST_CASE("Vertex storage formats", "[General]") {
  const float comparisonTolerance = 1.0f;

  auto testRenderer = createTestRenderer();
  Renderer &renderer = *testRenderer->renderer.get();

  geom::SphereGenerator sphere;
  sphere.radius = 1.0f;
  sphere.generate();

  struct BaseVert {
    float position[3];
    BaseVert(const geom::VertexPNT &other) { memcpy(position, other.position, sizeof(position)); }
  };

  std::vector<MeshVertexAttribute> baseAttributes{MeshVertexAttribute("position", 3)};
  std::vector<BaseVert> positions;
  for (auto &vert : sphere.vertices) {
    positions.emplace_back(vert);
  }

  DrawQueuePtr queue = std::make_shared<DrawQueue>();

  const int startX = 0;
  const int endX = 3;
  const float spacing = 2.25f;
  int X = 0;
  int Y = 0;
  auto appendMesh = [&](const MeshPtr &mesh) {
    float4x4 transform = linalg::translation_matrix(float3(spacing * (float(X) - 1.5f), spacing * (float(Y) - 1.0f), 0.0f));
    DrawablePtr drawable = std::make_shared<MeshDrawable>(mesh, transform);
    queue->add(drawable);

    X += 1;
    if (X > endX) {
      X = startX;
      Y++;
    }
  };

  ViewPtr view = std::make_shared<View>();
  view->proj = ViewOrthographicProjection{
      .size = spacing * 3.0f + 0.2f,
      .sizeType = OrthographicSizeType::Vertical,
      .near = -4.0f,
      .far = 4.0f,
  };

  visitTestVertexFormats([&](auto &&desc) {
    using Type = std::decay_t<decltype(desc)>;
    using ValueType = typename Type::ValueType;
    std::vector<uint8_t> buffer;

    static_assert(std::is_same_v<geom::SphereGenerator::index_t, uint16_t>);
    MeshFormat format{
        .indexFormat = IndexFormat::UInt16,
        .vertexAttributes = baseAttributes,
    };
    std::vector<MeshVertexAttribute> &attributes = format.vertexAttributes;
    attributes.emplace_back("color", 4, desc.storageType);

    buffer.resize((sizeof(BaseVert) + sizeof(ValueType) * 4) * positions.size());
    uint8_t *ptr = buffer.data();
    for (size_t i = 0; i < positions.size(); i++) {
      memcpy(ptr, &positions[i], sizeof(BaseVert));
      ptr += sizeof(BaseVert);
      for (size_t i = 0; i < 4; i++) {
        if (i == 1)
          memcpy(ptr, &desc.zero, sizeof(ValueType));
        else if (i < 3)
          memcpy(ptr, &desc.value, sizeof(ValueType));
        else
          memcpy(ptr, &desc.max, sizeof(ValueType));
        ptr += sizeof(ValueType);
      }
    }

    auto mesh = std::make_shared<Mesh>();
    mesh->update(format, buffer.data(), buffer.size(), (uint8_t *)sphere.indices.data(),
                 sphere.indices.size() * sizeof(geom::SphereGenerator::index_t));
    appendMesh(mesh);
  });

  PipelineSteps steps = createTestPipelineSteps(queue);
  TEST_RENDER_LOOP(testRenderer) { renderer.render(view, steps); };
  CHECK(testRenderer->checkFrame("vertexStorageFormats", comparisonTolerance));

  testRenderer.reset();
}

TEST_CASE("Pipeline states", "[General]") {
  const float comparisonTolerance = 1.0f;

  auto testRenderer = createTestRenderer(int2(512, 512));
  Renderer &renderer = *testRenderer->renderer.get();

  geom::SphereGenerator sphere;
  sphere.radius = 1.0f;
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
  DrawablePtr drawable = std::make_shared<MeshDrawable>(redSphereMesh, transform);
  queue->add(drawable);

  transform = linalg::translation_matrix(float3(0.5f, 0.0f, -3.0f));
  drawable = std::make_shared<MeshDrawable>(greenSphereMesh, transform);
  queue->add(drawable);

  auto testBlendState = [&](const char *name, const BlendState &state) {
    FeaturePtr blendFeature = std::make_shared<Feature>();
    blendFeature->state.set_depthWrite(false);
    blendFeature->state.set_blend(state);

    PipelineSteps steps{
        makePipelineStep(RenderDrawablesStep{
            .drawQueue = queue,
            .sortMode = SortMode::BackToFront,
            .features =
                {
                    features::Transform::create(),
                    features::BaseColor::create(),
                    blendFeature,
                },
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
  MeshDrawable::Ptr drawable;

  transform = linalg::translation_matrix(float3(-2.0f, 0.0f, 0.0f));
  drawable = std::make_shared<MeshDrawable>(sphereMesh, transform);
  queue->add(drawable);

  transform = linalg::translation_matrix(float3(0.0f, 0.0f, 0.0f));
  drawable = std::make_shared<MeshDrawable>(sphereMesh, transform);
  drawable->parameters.set("baseColor", float4(1, 0, 0, 1));
  queue->add(drawable);

  auto material = std::make_shared<Material>();
  material->parameters.set("baseColor", float4(0, 1, 0, 1));

  transform = linalg::translation_matrix(float3(2.0f, 0.0f, 0.0f));
  drawable = std::make_shared<MeshDrawable>(sphereMesh, transform);
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
  MeshDrawable::Ptr drawable;

  // Texture with default sampler state (linear filtering)
  TexturePtr textureA = textureFromFile(resolveDataPath("shards/gfx/tests/assets/pixely_sprite.png").string().c_str());

  // Texture with modified sampler (point filtering)
  TexturePtr textureB = textureA->clone();
  textureB->initWithSamplerState(SamplerState{
      .filterMode = WGPUFilterMode_Nearest,
  });

  transform = linalg::translation_matrix(float3(-.5f, 0.0f, 0.0f));
  drawable = std::make_shared<MeshDrawable>(planeMesh, transform);
  drawable->parameters.set("baseColorTexture", textureA);
  queue->add(drawable);

  transform = linalg::translation_matrix(float3(.5f, 0.0f, 0.0f));
  drawable = std::make_shared<MeshDrawable>(planeMesh, transform);
  drawable->parameters.set("baseColorTexture", textureB);
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
