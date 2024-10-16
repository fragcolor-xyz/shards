#ifndef E2A54666_469F_422F_922F_0DA635AB1EA2
#define E2A54666_469F_422F_922F_0DA635AB1EA2

#include "../fwd.hpp"
#include "../linalg.hpp"
#include "../material.hpp"
#include "../drawable.hpp"
#include "../anim/path.hpp"

namespace gfx {

struct MeshTreeDrawable;
struct Skin {
  std::vector<anim::Path> joints;
  std::vector<float4x4> inverseBindMatrices;
  std::vector<float4x4> jointMatrices;
  AABounds bounds;
};

struct MeshDrawable final : public IDrawable, public std::enable_shared_from_this<MeshDrawable> {
  typedef std::shared_ptr<MeshDrawable> Ptr;

private:
  UniqueId id = getNextDrawableId();
  friend struct gfx::UpdateUniqueId<MeshDrawable>;
  size_t version{};

public:
  MeshPtr mesh;
  MaterialPtr material;
  std::vector<FeaturePtr> features;
  MaterialParameters parameters;
  float4x4 transform;

  // Optional skin attached to this mesh, it'll be used to transform this mesh's vertices
  std::shared_ptr<Skin> skin;

  std::optional<float4x4> previousTransform;

  // Clipping rectangle as (min, max)
  std::optional<int4> clipRect;

  MeshDrawable() = default;
  MeshDrawable(MeshPtr mesh, float4x4 transform = linalg::identity, MaterialPtr material = MaterialPtr())
      : mesh(mesh), material(material), transform(transform) {}

  DrawablePtr clone() const override;

  DrawablePtr self() const override { return const_cast<MeshDrawable *>(this)->shared_from_this(); }

  UniqueId getId() const override { return id; }

  DrawableProcessorConstructor getProcessor() const override;

  void update() { ++version; };
  size_t getVersion() const override { return version; }
  OrientedBounds getBounds() const override;
};
} // namespace gfx

#endif /* E2A54666_469F_422F_922F_0DA635AB1EA2 */
