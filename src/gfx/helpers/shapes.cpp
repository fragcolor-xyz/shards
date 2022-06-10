#include "shapes.hpp"

namespace gfx {

const std::vector<MeshVertexAttribute> &ShapeVertex::getAttributes() {
  static std::vector<MeshVertexAttribute> attribs = []() {
    std::vector<MeshVertexAttribute> attribs;
    attribs.emplace_back("position", 3, VertexAttributeType::Float32);
    attribs.emplace_back("color", 4, VertexAttributeType::Float32);
    attribs.emplace_back("direction", 3, VertexAttributeType::Float32);
    attribs.emplace_back("offsetSS", 2, VertexAttributeType::Float32);
    return attribs;
  }();
  return attribs;
}

// 4 vertices are used to draw a line segment
// 2 placed at the start point and 2 at the end point
// The position & fdata attribute is set as follows along the quad:
//  [a, 1]---------------------------[b, 1]    y
//    |                                |       |
//  [a, -1]--------------------------[b,-1]    .___ x
// The vertices are then extruded along the y direction in screen space
// the extrusion amount is based on the line thickness which is stored in udata[0] for each vertex
FeaturePtr ScreenSpaceSizeFeature::create() {
  FeaturePtr result = std::make_shared<Feature>();
  result->state.set_culling(false);

  shader::EntryPoint &entry =
      result->shaderEntryPoints.emplace_back("screenSpaceLineGeometry", ProgrammableGraphicsStage::Vertex, shader::BlockPtr());

  using namespace gfx::shader::blocks;
  using namespace gfx::shader;
  std::unique_ptr<Compound> code = makeCompoundBlock();
  code->appendLine("var offsetSS =", ReadInput("offsetSS"));
  code->appendLine("var dir = ", ReadInput("direction"));
  code->appendLine("var color = ", ReadInput("color"));
  code->appendLine("var posWS = ", ReadInput("position"));
  code->appendLine("var world = ", ReadBuffer("world", FieldTypes::Float4x4));
  code->appendLine("var view = ", ReadBuffer("view", FieldTypes::Float4x4, "view"));
  code->appendLine("var invView = ", ReadBuffer("invView", FieldTypes::Float4x4, "view"));
  code->appendLine("var proj = ", ReadBuffer("proj", FieldTypes::Float4x4, "view"));
  code->appendLine("var viewport = ", ReadBuffer("viewport", FieldTypes::Float4, "view"));
  code->appendLine("var cameraPosition = invView[3].xyz");
  code->appendLine("var nextWS = posWS+", ReadInput("direction"));
  code->appendLine("var nextProj = proj* view * world * vec4<f32>(nextWS, 1.0)");
  code->appendLine("var nextNDC = nextProj.xyz / nextProj.w");
  code->appendLine("var posProj = proj* view * world * vec4<f32>(posWS, 1.0)");
  code->appendLine("var posNDC = posProj.xyz / posProj.w");
  code->appendLine("var directionSS = normalize(nextNDC.xy - posNDC.xy)"); // Direction of the line in screen space
  code->appendLine("var tangentSS = vec2<f32>(-directionSS.y, directionSS.x)");
  code->appendLine("posProj.x = (posNDC.x + tangentSS.x * offsetSS.y * (1.0/viewport.z) + directionSS.x * offsetSS.x * "
                   "(1.0/viewport.z)) * posProj.w");
  code->appendLine("posProj.y = (posNDC.y + tangentSS.y * offsetSS.y * (1.0/viewport.w) + directionSS.y * offsetSS.x *"
                   "(1.0/viewport.w)) * posProj.w");
  code->append(WriteOutput("position", FieldTypes::Float4, "posProj"));
  entry.code = std::move(code);

  return result;
}

#define UNPACK3(_x) \
  { _x.x, _x.y, _x.z }
#define UNPACK4(_x) \
  { _x.x, _x.y, _x.z, _x.w }

void ShapeRenderer::addLine(float3 a, float3 b, float3 dirA, float3 dirB, float4 color, uint32_t thickness) {
  float xOffset = 0.5f * thickness;
  float yOffset = 1.0f * thickness;
  vertices.push_back(ShapeVertex{
      .position = UNPACK3(a),
      .color = UNPACK4(color),
      .direction = UNPACK3(dirA),
      .offsetSS = {-xOffset, yOffset},
  });
  vertices.push_back(ShapeVertex{
      .position = UNPACK3(b),
      .color = UNPACK4(color),
      .direction = UNPACK3(dirB),
      .offsetSS = {xOffset, yOffset},
  });
  vertices.push_back(ShapeVertex{
      .position = UNPACK3(b),
      .color = UNPACK4(color),
      .direction = UNPACK3(dirB),
      .offsetSS = {xOffset, -yOffset},
  });

  vertices.push_back(ShapeVertex{
      .position = UNPACK3(a),
      .color = UNPACK4(color),
      .direction = UNPACK3(dirA),
      .offsetSS = {-xOffset, -yOffset},
  });
  vertices.push_back(ShapeVertex{
      .position = UNPACK3(a),
      .color = UNPACK4(color),
      .direction = UNPACK3(dirA),
      .offsetSS = {-xOffset, yOffset},
  });
  vertices.push_back(ShapeVertex{
      .position = UNPACK3(b),
      .color = UNPACK4(color),
      .direction = UNPACK3(dirB),
      .offsetSS = {xOffset, -yOffset},
  });
}

void ShapeRenderer::addLine(float3 a, float3 b, float4 color, uint32_t thickness) {
  float3 direction = linalg::normalize(b - a);
  addLine(a, b, direction, direction, color, thickness);
}

void ShapeRenderer::addCircle(float3 center, float3 xBase, float3 yBase, float radius, float4 color, uint32_t thickness,
                              uint32_t resolution) {
  float3 prevPos;
  float3 prevDelta;
  for (size_t i = 0; i < resolution; i++) {
    float t = i / float(resolution - 1) * pi2;
    float tCos = std::cosf(t);
    float tSin = std::sinf(t);
    float3 pos = center + tCos * xBase * radius + tSin * yBase * radius;
    float3 delta = center + -tSin * xBase + tCos * yBase;
    if (i > 0) {
      addLine(prevPos, pos, prevDelta, delta, color, thickness);
    }
    prevPos = pos;
    prevDelta = delta;
  }
}

void ShapeRenderer::addRect(float3 center, float3 xBase, float3 yBase, float2 size, float4 color, uint32_t thickness) {
  float2 halfSize = size / 2.0f;
  float3 verts[] = {
      center - halfSize.x * xBase - halfSize.y * yBase,
      center + halfSize.x * xBase - halfSize.y * yBase,
      center + halfSize.x * xBase + halfSize.y * yBase,
      center - halfSize.x * xBase + halfSize.y * yBase,
  };

  for (size_t i = 0; i < 4; i++) {
    float3 a = verts[i];
    float3 b = verts[(i + 1) % 4];
    addLine(a, b, color, thickness);
  }
}

void ShapeRenderer::addBox(float3 center, float3 xBase, float3 yBase, float3 zBase, float3 size, float4 color,
                           uint32_t thickness) {
  float3 halfSize = size / 2.0f;
  float3 verts[] = {
      center - halfSize.x * xBase - halfSize.y * yBase - halfSize.z * zBase,
      center + halfSize.x * xBase - halfSize.y * yBase - halfSize.z * zBase,
      center + halfSize.x * xBase + halfSize.y * yBase - halfSize.z * zBase,
      center - halfSize.x * xBase + halfSize.y * yBase - halfSize.z * zBase,

      center - halfSize.x * xBase - halfSize.y * yBase + halfSize.z * zBase,
      center + halfSize.x * xBase - halfSize.y * yBase + halfSize.z * zBase,
      center + halfSize.x * xBase + halfSize.y * yBase + halfSize.z * zBase,
      center - halfSize.x * xBase + halfSize.y * yBase + halfSize.z * zBase,
  };

  auto drawFace4 = [&](size_t start) {
    for (size_t i = 0; i < 4; i++) {
      float3 a = verts[start + i];
      float3 b = verts[start + (i + 1) % 4];
      addLine(a, b, color, thickness);
    }
  };
  drawFace4(0);
  drawFace4(4);
  for (size_t i = 0; i < 4; i++) {
    float3 a = verts[i];
    float3 b = verts[i + 4];
    addLine(a, b, color, thickness);
  }
}

void ShapeRenderer::begin() { vertices.clear(); }

void ShapeRenderer::end(DrawQueuePtr queue) {
  if (!mesh)
    mesh = std::make_shared<Mesh>();
  MeshFormat fmt = {
      .primitiveType = PrimitiveType::TriangleList,
      .windingOrder = WindingOrder::CCW,
      .vertexAttributes = ShapeVertex::getAttributes(),
  };
  mesh->update(fmt, vertices.data(), vertices.size() * sizeof(ShapeVertex), nullptr, 0);

  if (!drawable)
    drawable = std::make_shared<Drawable>(mesh);
  queue->add(drawable);
}

} // namespace gfx
