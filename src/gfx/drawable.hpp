#ifndef B908F41D_8437_47C0_B3F9_FA0AE98387FC
#define B908F41D_8437_47C0_B3F9_FA0AE98387FC

#include "fwd.hpp"
#include "linalg.hpp"
#include "material.hpp"
#include "hasherxxh128.hpp"
#include <memory>
#include <vector>

namespace gfx {

typedef size_t DrawableGeneration;

struct HashCollector;
struct IDrawable {
  virtual ~IDrawable() = default;

  // Duplicate self
  virtual DrawablePtr clone() const = 0;

  // Get the processor used to render this drawable
  virtual IDrawableProcessor &getProcessor() const = 0;

  // Compute hash and collect references
  // The drawable should not be modified while it is being processed by the renderer
  // After Renderer::render returns you are free to change this drawable again
  virtual void staticHashCollect(HashCollector &hashCollector) const = 0;

  // Unique Id to identify this drawable
  virtual UniqueId getId() const = 0;
};

template <typename T> inline std::shared_ptr<T> clone(const std::shared_ptr<T> &other) {
  return std::static_pointer_cast<T>(other->clone());
}

UniqueId getNextDrawableId();

struct DrawQueue {
private:
  std::vector<DrawablePtr> sharedDrawables;
  std::vector<const IDrawable *> drawables;

public:
  // Adds a managed drawable, automatically kept alive until the renderer is done with it
  void add(const DrawablePtr &drawable) {
    sharedDrawables.push_back(drawable);
    drawables.push_back(drawable.get());
  }

  // Add an external drawable, you are responsible for keeping the object alive until it has been submitted to the renderer
  void add(const IDrawable &drawable) { drawables.push_back(&drawable); }

  void clear() { drawables.clear(); }

  const std::vector<DrawablePtr> &getSharedDrawables() const { return sharedDrawables; }
  const std::vector<const IDrawable *> &getDrawables() const { return drawables; }
};

} // namespace gfx

#endif /* B908F41D_8437_47C0_B3F9_FA0AE98387FC */
