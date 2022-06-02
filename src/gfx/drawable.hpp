#ifndef GFX_DRAWABLE
#define GFX_DRAWABLE

#include "fwd.hpp"
#include "linalg.hpp"
#include "linalg/linalg.h"
#include "material.hpp"
#include <memory>
#include <vector>

namespace gfx {
struct Drawable {
  typedef std::shared_ptr<Drawable> PtrType;

  MeshPtr mesh;
  MaterialPtr material;
  std::vector<FeaturePtr> features;
  MaterialParameters parameters;
  float4x4 transform;

  Drawable(MeshPtr mesh, float4x4 transform = linalg::identity, MaterialPtr material = MaterialPtr())
      : mesh(mesh), material(material), transform(transform) {}

  PtrType clone() const;
};

// Wraps Drawable in a classic transform tree
struct DrawableHierarchy {
  typedef std::shared_ptr<DrawableHierarchy> PtrType;

  std::vector<PtrType> children;
  std::vector<DrawablePtr> drawables;
  float4x4 transform = linalg::identity;
  std::string label;

  PtrType clone() const;
};

struct DrawQueue {
private:
  std::vector<DrawablePtr> drawables;

public:
  void add(DrawablePtr drawable) { drawables.push_back(drawable); }
  void add(DrawableHierarchyPtr hierarchy);
  void clear() { drawables.clear(); }
  const std::vector<DrawablePtr> &getDrawables() const { return drawables; }
};

} // namespace gfx

#endif // GFX_DRAWABLE
