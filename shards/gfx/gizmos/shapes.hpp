#ifndef D0FA649B_29CC_4C2E_8A09_F3142788BE28
#define D0FA649B_29CC_4C2E_8A09_F3142788BE28

#include "../drawable.hpp"
#include "../feature.hpp"
#include "../linalg.hpp"
#include "../mesh.hpp"
#include "../shader/blocks.hpp"
#include "../../core/pool.hpp"

namespace gfx {

struct ScreenSpaceSizeFeature {
  static FeaturePtr create();
};

struct NoCullingFeature {
  static FeaturePtr create();
};

struct GizmoLightingFeature {
  static FeaturePtr create();
};

struct ShapeRenderer {
public:
  struct LineVertex {
    float position[3];
    float color[4] = {1, 1, 1, 1};
    float direction[4] = {};
    float offsetSS[2] = {};

    void setPosition(const float3 &position) { memcpy(this->position, &position.x, sizeof(float) * 3); }
    void setColor(const float4 &color) { memcpy(this->color, &color.x, sizeof(float) * 4); }
    void setNormal(const float3 &direction) { memcpy(this->direction, &direction.x, sizeof(float) * 3); }
    void setLength(float len) { this->direction[3] = len; }
    void setScreenSpaceOffset(float2 offsetSS) { memcpy(this->offsetSS, &offsetSS.x, sizeof(float) * 2); }

    static const std::vector<MeshVertexAttribute> &getAttributes();
  };

  struct SolidVertex {
    float position[3];
    float color[4] = {1, 1, 1, 1};

    void setPosition(const float3 &position) { memcpy(this->position, &position.x, sizeof(float) * 3); }
    void setColor(const float4 &color) { memcpy(this->color, &color.x, sizeof(float) * 4); }

    static const std::vector<MeshVertexAttribute> &getAttributes();
  };

  struct TextVertex {
    float position[3];
    float color[4] = {1, 1, 1, 1};
    float uv[2] = {};

    void setPosition(const float3 &position) { memcpy(this->position, &position.x, sizeof(float) * 3); }
    void setColor(const float4 &color) { memcpy(this->color, &color.x, sizeof(float) * 4); }
    void setUV(const float2 &uv) { memcpy(this->uv, &uv.x, sizeof(float) * 2); }

    static const std::vector<MeshVertexAttribute> &getAttributes();
  };

private:
  FeaturePtr screenSpaceSizeFeature = ScreenSpaceSizeFeature::create();
  FeaturePtr noCullingFeature = NoCullingFeature::create();

  std::vector<LineVertex> lineVertices;
  std::vector<SolidVertex> solidVertices;
  std::vector<SolidVertex> unculledSolidVertices;
  std::vector<TextVertex> textVertices;
  shards::Pool<MeshPtr> lineMeshPool;
  shards::Pool<MeshPtr> solidMeshPool;
  shards::Pool<MeshPtr> unculledSolidMeshPool;
  shards::Pool<MeshPtr> textMeshPool;
  std::vector<DrawablePtr> custom;

public:
  void addLine(float3 a, float3 b, float3 dirA, float3 dirB, float4 color, float thickness);
  void addLine(float3 a, float3 b, float4 color, float thickness);
  void addCircle(float3 center, float3 xBase, float3 yBase, float radius, float4 color, float thickness, uint32_t resolution);
  void addRect(float3 center, float3 xBase, float3 yBase, float2 size, float4 color, float thickness);
  void addBox(float3 center, float3 xBase, float3 yBase, float3 zBase, float3 size, float4 color, float thickness);
  void addBox(float4x4 transform, float3 center, float3 size, float4 color, float thickness);
  void addPoint(float3 center, float4 color, float thickness);

  void addSolidRect(float3 center, float3 xBase, float3 yBase, float2 size, float4 color, bool culling = true);
  void addSolidQuad(float3 a, float3 b, float3 c, float3 d, float4 color, bool culling = true);
  void addDisc(float3 center, float3 xBase, float3 yBase, float outerRadius, float innerRadius, float4 color, bool culling = true,
               uint32_t resolution = 64);

  void addText(float3 origin, float3 xBase, float3 yBase, float size, std::string_view text, float4 color, bool center);

  void addSolidTriangle(float3 a, float3 b, float3 c, float4 color, bool culling = true);

  void addCustom(DrawablePtr drawable) { custom.push_back(drawable); }

  void begin();
  void end(DrawQueuePtr queue);
};

struct GizmoRenderer {
private:
  ViewPtr view;
  float2 viewportSize;

  MeshPtr handleBodyMesh;
  MeshPtr arrowMesh;
  MeshPtr cubeMesh;
  MeshPtr sphereMesh;

  // Moves cylinders origin to bottom cap center and point orient top cap facing z+
  float4x4 cylinderAdjustment;

  std::vector<DrawablePtr> drawables;

  ShapeRenderer shapeRenderer;

  FeaturePtr gizmoLightingFeature = GizmoLightingFeature::create();

public:
  float scalingFactor = 1.0f;

public:
  GizmoRenderer();

  // Factor to scale by for constant screen-space size
  // float getSize(float3 position) const;
  float getConstantScreenSize(float3 position, float size) const;

  struct BillboardParams {
    float3 x, y;
  };
  BillboardParams getBillboard(float3 position) const;

  enum class CapType { Cube, Arrow, Sphere };

  void addHandle(float3 origin, float3 direction, float radius, float length, float4 bodyColor, CapType capType, float4 capColor);
  void addCubeHandle(float3 center, float size, float4 color);

  void addTextBillboard(float3 origin, std::string_view text, float4 color, float size, bool center);

  void begin(ViewPtr view, float2 viewportSize);
  void end(DrawQueuePtr queue);

  struct HandleGeometry {
    float4x4 bodyTransform;
    float4x4 capTransform;
  };
  // bodyRatio = radius of body relative to handle radius
  // capRatio = height of the cap relative to handle radius*2
  // extendBodyToCapCenter = extend body into the cap center
  HandleGeometry generateHandleGeometry(float3 origin, float3 direction, float radius, float length, float bodyRatio,
                                        float capRatio, bool extendBodyToCapCenter);

  ShapeRenderer &getShapeRenderer() { return shapeRenderer; }

private:
  void loadGeometry();
};
} // namespace gfx

#endif /* D0FA649B_29CC_4C2E_8A09_F3142788BE28 */
