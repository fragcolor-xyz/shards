#ifndef B908F41D_8437_47C0_B3F9_FA0AE98387FC
#define B908F41D_8437_47C0_B3F9_FA0AE98387FC

#include "fwd.hpp"
#include "linalg.hpp"
#include "material.hpp"
#include "hasherxxh3.hpp"
#include <shards/core/pmr/wrapper.hpp>
#include <shards/core/pmr/vector.hpp>
#include "bounds.hpp"
#include <memory>
#include <vector>

namespace gfx {
namespace detail {
struct PipelineHashCollector;
}

typedef detail::DrawableProcessorPtr (*DrawableProcessorConstructor)(Context &);

struct IDrawable {
  virtual ~IDrawable() = default;

  // Duplicate self
  virtual DrawablePtr clone() const = 0;

  // Pointer to self
  virtual DrawablePtr self() const = 0;

  // Unique Id to identify this drawable
  virtual UniqueId getId() const = 0;

  virtual size_t getVersion() const {
    throw std::logic_error("Drawable does not have a version, most likely needs to be expanded first");
  }

  virtual OrientedBounds getBounds() const {
    throw std::logic_error("Drawable does not have bounds, most likely needs to be expanded first");
  }

  // If this is a group this function should extract it's contents and return true
  virtual bool expand(shards::pmr::vector<const IDrawable *> &outDrawables, shards::pmr::PolymorphicAllocator<> alloc) const { return false; }

  // virtual void ShapeRenderer

  // Get the processor used to render this drawable
  virtual DrawableProcessorConstructor getProcessor() const = 0;
};

template <typename T> inline std::shared_ptr<T> clone(const std::shared_ptr<T> &other) {
  return std::static_pointer_cast<T>(other->clone());
}

UniqueId getNextDrawableId();
UniqueId getNextDrawQueueId();

struct DrawQueue {
private:
  std::vector<DrawablePtr> sharedDrawables;
  std::vector<const IDrawable *> drawables;
  bool autoClear{};
  UniqueId id = getNextDrawQueueId();

public:
  // Enable to trace rendering operations on this queue
  bool trace{};

public:
  // Adds a managed drawable, automatically kept alive until the renderer is done with it
  void add(const DrawablePtr &drawable) {
    sharedDrawables.push_back(drawable);
    drawables.push_back(drawable.get());
  }

  // Add an external drawable, you are responsible for keeping the object alive until it has been submitted to the renderer
  void add(const IDrawable &drawable) { drawables.push_back(&drawable); }

  void clear() {
    drawables.clear();
    sharedDrawables.clear();
  }

  UniqueId getId() const { return id; }

  bool isAutoClear() const { return autoClear; }
  void setAutoClear(bool autoClear) { this->autoClear = autoClear; }

  const std::vector<DrawablePtr> &getSharedDrawables() const { return sharedDrawables; }
  const std::vector<const IDrawable *> &getDrawables() const { return drawables; }
};

} // namespace gfx

#endif /* B908F41D_8437_47C0_B3F9_FA0AE98387FC */
