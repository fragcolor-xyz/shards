// Tests for editor-related functionality
// such as gizmos,highlights,debug lines

#include "./context.hpp"
#include "./data.hpp"
#include "./renderer.hpp"
#include "renderer_utils.hpp"
#include <catch2/catch_all.hpp>
#include <gfx/context.hpp>
#include <gfx/drawables/mesh_drawable.hpp>
#include <gfx/features/wireframe.hpp>
#include <gfx/steps/defaults.hpp>
#include <gfx/gizmos/shapes.hpp>
#include <gfx/gizmos/wireframe.hpp>

using namespace gfx;
using namespace gfx::steps;

static constexpr float comparisonTolerance = 0.05f;

TEST_CASE("Wireframe", "[Gizmos]") {
  auto testRenderer = createTestRenderer();
  Renderer &renderer = *testRenderer->renderer.get();

  MeshPtr cubeMesh = createCubeMesh();

  ViewPtr view = std::make_shared<View>();
  view->view = linalg::lookat_matrix(float3(3, 3.0f, 3.0f), float3(0, 0, 0), float3(0, 1, 0));
  view->proj = ViewPerspectiveProjection{
      degToRad(45.0f),
      FovDirection::Horizontal,
  };

  DrawQueuePtr queue = std::make_shared<DrawQueue>();
  DrawQueuePtr editorQueue = std::make_shared<DrawQueue>();
  float4x4 transform;
  MeshDrawable::Ptr drawable;

  transform = linalg::translation_matrix(float3(0.0f, 0.0f, 0.0f));
  drawable = std::make_shared<MeshDrawable>(cubeMesh, transform);
  drawable->parameters.set("baseColor", float4(0.2f, 0.2f, 0.2f, 1.0f));

  PipelineSteps steps{
      makePipelineStep(RenderDrawablesStep{
          .drawQueue = queue,
          .features =
              {
                  features::Transform::create(),
                  features::BaseColor::create(),
              },
      }),
      makePipelineStep(RenderDrawablesStep{
          .drawQueue = editorQueue,
          .features =
              {
                  features::Transform::create(),
                  features::BaseColor::create(),
              },
          .output = getDefaultRenderStepOutput(false, std::nullopt),
      }),
  };

  WireframeRenderer wr0(false);
  WireframeRenderer wr1(true);

  auto loop = [&](WireframeRenderer &wr) {
    queue->clear();
    editorQueue->clear();

    queue->add(drawable);
    wr.overlayWireframe(*editorQueue.get(), *drawable.get(), float4(1.0f));

    renderer.render(view, steps);
  };

  TEST_RENDER_LOOP(testRenderer) { loop(wr0); };
  CHECK(testRenderer->checkFrame("wireframe", comparisonTolerance));

  TEST_RENDER_LOOP(testRenderer) { loop(wr1); };
  CHECK(testRenderer->checkFrame("wireframe-backfaces", comparisonTolerance));
}

TEST_CASE("Helper lines", "[Gizmos]") {
  auto testRenderer = createTestRenderer();
  Renderer &renderer = *testRenderer->renderer.get();

  ViewPtr view = std::make_shared<View>();
  view->view = linalg::lookat_matrix(float3(3.0f, 3.0f, 3.0f), float3(0, 0, 0), float3(0, 1, 0));
  view->proj = ViewPerspectiveProjection{
      degToRad(45.0f),
      FovDirection::Horizontal,
  };

  MeshPtr cubeMesh = createCubeMesh();
  float4x4 transform;
  MeshDrawable::Ptr cubeDrawable;

  transform = linalg::translation_matrix(float3(0.0f, 0.0f, 0.0f));
  cubeDrawable = std::make_shared<MeshDrawable>(cubeMesh, transform);
  cubeDrawable->parameters.set("baseColor", float4(0.2f, 0.2f, 0.2f, 1.0f));

  DrawQueuePtr baseQueue = std::make_shared<DrawQueue>();
  DrawQueuePtr editorQueue = std::make_shared<DrawQueue>();

  PipelineSteps steps{
      makePipelineStep(RenderDrawablesStep{
          .drawQueue = baseQueue,
          .features =
              {
                  features::Transform::create(),
                  features::BaseColor::create(),
              },
      }),
      makePipelineStep(RenderDrawablesStep{
          .drawQueue = editorQueue,
          .features =
              {
                  features::BaseColor::create(),
              },
          .output = getDefaultRenderStepOutput(false, std::nullopt),
      }),
  };

  ShapeRenderer sr;
  TEST_RENDER_LOOP(testRenderer) {
    baseQueue->clear();
    editorQueue->clear();

    // Cube to test depth buffer interaction
    baseQueue->add(cubeDrawable);

    sr.begin();
    size_t numSteps = 8;
    float spacing = 1.0f / 8.0f;
    for (size_t i = 0; i < numSteps; i++) {
      float sz = (float(i)) * spacing;
      sr.addLine(float3(0, 0, sz), float3(1, 0, sz), float4(1, 0, 0, 1), 1 + i);
      sr.addLine(float3(0, 0, sz), float3(1, 0, sz), float4(1, 0, 0, 1), 1 + i);

      sr.addLine(float3(sz, 0, 0), float3(sz, 1, 0), float4(0, 1, 0, 1), 1 + i);
      sr.addLine(float3(sz, 0, 0), float3(sz, 1, 0), float4(0, 1, 0, 1), 1 + i);

      sr.addLine(float3(0, sz, 0), float3(0, sz, 1), float4(0, 0, 1, 1), 1 + i);
      sr.addLine(float3(0, sz, 0), float3(0, sz, 1), float4(0, 0, 1, 1), 1 + i);
    }
    sr.end(editorQueue);
    renderer.render(view, steps);
  };

  CHECK(testRenderer->checkFrame("helper_lines", comparisonTolerance));
}

TEST_CASE("Helper circles", "[Gizmos]") {
  auto testRenderer = createTestRenderer();
  Renderer &renderer = *testRenderer->renderer.get();

  ViewPtr view = std::make_shared<View>();
  view->view = linalg::lookat_matrix(float3(3.0f, 3.0f, 3.0f), float3(0, 0, 0), float3(0, 1, 0));
  view->proj = ViewPerspectiveProjection{
      degToRad(45.0f),
      FovDirection::Horizontal,
  };

  DrawQueuePtr editorQueue = std::make_shared<DrawQueue>();

  PipelineSteps steps{
      makePipelineStep(RenderDrawablesStep{
          .drawQueue = editorQueue,
          .features =
              {
                  features::BaseColor::create(),
              },
      }),
  };

  ShapeRenderer sr;
  TEST_RENDER_LOOP(testRenderer) {
    editorQueue->clear();

    sr.begin();
    for (size_t i = 0; i < 8; i++) {
      float r = 0.2f + (0.2f * i);
      uint32_t thickness = (1 + i);
      uint32_t res = 8 + (16 * i);
      sr.addCircle(float3(0, 0, 0), float3(1, 0, 0), float3(0, 0, 1), r - 0.0f, float4(0, 1, 0, 1), thickness, res);
      sr.addCircle(float3(0, 0, 0), float3(0, 0, 1), float3(0, 1, 0), r - 0.05f, float4(1, 0, 0, 1), thickness, res);
      sr.addCircle(float3(0, 0, 0), float3(1, 0, 0), float3(0, 1, 0), r - 0.1f, float4(0, 0, 1, 1), thickness, res);
    }
    sr.end(editorQueue);
    renderer.render(view, steps);
  };

  CHECK(testRenderer->checkFrame("helper_circles", comparisonTolerance));
}

TEST_CASE("Helper rectangles", "[Gizmos]") {
  auto testRenderer = createTestRenderer();
  Renderer &renderer = *testRenderer->renderer.get();

  ViewPtr view = std::make_shared<View>();
  view->view = linalg::lookat_matrix(float3(3.0f, 3.0f, 3.0f), float3(0, 0, 0), float3(0, 1, 0));
  view->proj = ViewPerspectiveProjection{
      degToRad(45.0f),
      FovDirection::Horizontal,
  };

  DrawQueuePtr editorQueue = std::make_shared<DrawQueue>();

  PipelineSteps steps{
      makePipelineStep(RenderDrawablesStep{
          .drawQueue = editorQueue,
          .features =
              {
                  features::BaseColor::create(),
              },
      }),
  };

  ShapeRenderer sr;
  TEST_RENDER_LOOP(testRenderer) {
    editorQueue->clear();

    sr.begin();
    for (size_t i = 0; i < 5; i++) {
      float r = 0.2f + (0.2f * i);
      float2 size = float2(r - 0.1f, r + 0.1f);
      uint32_t thickness = (1 + i);
      sr.addRect(float3(0, 0, 0), float3(1, 0, 0), float3(0, 0, 1), size, float4(0, 1, 0, 1), thickness);
      sr.addRect(float3(0, 0, 0), float3(0, 0, 1), float3(0, 1, 0), size, float4(1, 0, 0, 1), thickness);
      sr.addRect(float3(0, 0, 0), float3(1, 0, 0), float3(0, 1, 0), size, float4(0, 0, 1, 1), thickness);
    }
    sr.end(editorQueue);
    renderer.render(view, steps);
  };

  CHECK(testRenderer->checkFrame("helper_rectangles", comparisonTolerance));
}

TEST_CASE("Helper boxes", "[Gizmos]") {
  auto testRenderer = createTestRenderer();
  Renderer &renderer = *testRenderer->renderer.get();

  ViewPtr view = std::make_shared<View>();
  view->view = linalg::lookat_matrix(float3(3.0f, 3.0f, 3.0f), float3(0, 0, 0), float3(0, 1, 0));
  view->proj = ViewPerspectiveProjection{
      degToRad(45.0f),
      FovDirection::Horizontal,
  };

  DrawQueuePtr editorQueue = std::make_shared<DrawQueue>();

  PipelineSteps steps{
      makePipelineStep(RenderDrawablesStep{
          .drawQueue = editorQueue,
          .features =
              {
                  features::BaseColor::create(),
              },
      }),
  };

  float3 axisX = float3(1, 0, 0);
  float3 axisY = float3(0, 1, 0);
  float3 axisZ = float3(0, 0, 1);

  ShapeRenderer sr;
  TEST_RENDER_LOOP(testRenderer) {
    editorQueue->clear();

    sr.begin();
    for (size_t i = 0; i < 3; i++) {
      float r = 0.2f + (0.2f * i);
      float3 size = float3(r - 0.1f, r + 0.1f, r + 0.2f);
      uint32_t thickness = (1 + i);
      sr.addBox(float3(0, 0, 0), axisX, axisY, axisZ, size, float4(1, 1, 1, 1), thickness);
    }
    sr.end(editorQueue);
    renderer.render(view, steps);
  };

  CHECK(testRenderer->checkFrame("helper_boxes", comparisonTolerance));
}

TEST_CASE("Gizmo handles", "[Gizmos]") {
  auto testRenderer = createTestRenderer();
  Renderer &renderer = *testRenderer->renderer.get();

  ViewPtr view = std::make_shared<View>();
  view->view = linalg::lookat_matrix(float3(3.0f, 3.0f, 3.0f), float3(0, 0, 0), float3(0, 1, 0));
  view->proj = ViewPerspectiveProjection{
      degToRad(45.0f),
      FovDirection::Horizontal,
  };

  DrawQueuePtr editorQueue = std::make_shared<DrawQueue>();

  PipelineSteps steps{
      makePipelineStep(RenderDrawablesStep{
          .drawQueue = editorQueue,
          .features =
              {
                  features::Transform::create(),
                  features::BaseColor::create(),
              },
      }),
  };

  float3 axisX = float3(1, 0, 0);
  float3 axisY = float3(0, 1, 0);
  float3 axisZ = float3(0, 0, 1);

  GizmoRenderer gr;
  TEST_RENDER_LOOP(testRenderer) {
    editorQueue->clear();

    int2 viewportSize = renderer.getViewStack().getOutput().size;
    gr.begin(view, float2(viewportSize));
    float4 colors[3] = {
        float4(1, 0, 0, 1),
        float4(0, 1, 0, 1),
        float4(0, 0, 1, 1),
    };
    for (size_t i = 0; i < 3; i++) {
      float length = (1.0f + float(i) * 0.2f);
      float radius = 0.1f;
      float xOffset = 0.0f + float(i) * 0.2f;
      gr.addHandle(float3(xOffset, 0, 0), axisY, radius, length, colors[i], GizmoRenderer::CapType::Arrow, colors[i]);
      gr.addHandle(float3(xOffset * 0.5f, 0, 0.5f), axisY, radius * 0.5f, length * 0.5f, float4(1, 1, 1, 1),
                   GizmoRenderer::CapType::Arrow, colors[i]);
    }
    gr.end(editorQueue);
    renderer.render(view, steps);
  };

  CHECK(testRenderer->checkFrame("helper_handles", comparisonTolerance));
}
