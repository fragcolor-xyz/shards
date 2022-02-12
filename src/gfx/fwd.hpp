#pragma once

namespace gfx {

struct Drawable;
struct Mesh;
struct Feature;
struct View;
struct Material;
typedef std::shared_ptr<Drawable> DrawablePtr;
typedef std::shared_ptr<Mesh> MeshPtr;
typedef std::shared_ptr<Feature> FeaturePtr;
typedef std::shared_ptr<View> ViewPtr;
typedef std::shared_ptr<Material> MaterialPtr;

} // namespace gfx
