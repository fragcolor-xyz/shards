#pragma once
#include "fwd.hpp"
#include "linalg.hpp"
#include "linalg/linalg.h"
#include "material.hpp"
#include <memory>
#include <vector>

namespace gfx {
struct Drawable {
  MeshPtr mesh;
  MaterialPtr material;
  std::vector<FeaturePtr> features;
  MaterialParameters parameters;
  float4x4 transform;

  Drawable(MeshPtr mesh, float4x4 transform = linalg::identity, MaterialPtr material = MaterialPtr())
      : mesh(mesh), material(material), transform(transform) {}
};

struct DrawQueue {
private:
  std::vector<DrawablePtr> drawables;

public:
  void add(DrawablePtr drawable) { drawables.push_back(drawable); }
  void clear() { drawables.clear(); }
  const std::vector<DrawablePtr> &getDrawables() const { return drawables; }
};

} // namespace gfx
