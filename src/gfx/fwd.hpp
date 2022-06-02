#ifndef GFX_FWD
#define GFX_FWD

namespace gfx {

struct Context;
struct Pipeline;
struct DrawQueue;

struct Drawable;
typedef std::shared_ptr<Drawable> DrawablePtr;

struct DrawableHierarchy;
typedef std::shared_ptr<DrawableHierarchy> DrawableHierarchyPtr;

struct Mesh;
typedef std::shared_ptr<Mesh> MeshPtr;

struct Feature;
typedef std::shared_ptr<Feature> FeaturePtr;

struct View;
typedef std::shared_ptr<View> ViewPtr;

struct Material;
typedef std::shared_ptr<Material> MaterialPtr;

struct Texture;
typedef std::shared_ptr<Texture> TexturePtr;

} // namespace gfx

#endif // GFX_FWD
