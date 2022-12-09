#ifndef D4C41EFB_5953_4831_9CB1_B6CCD9650575
#define D4C41EFB_5953_4831_9CB1_B6CCD9650575

#include "mesh_drawable.hpp"
#include <cassert>
#include <set>

#ifndef NDEBUG
#define GFX_TRANSFORM_UPDATER_TRACK_VISITED 1
#endif

namespace gfx {
// Transform tree of drawable objects
struct MeshTreeDrawable final : public IDrawable {
  typedef std::shared_ptr<MeshTreeDrawable> Ptr;

private:
  UniqueId id = getNextDrawableId();
  friend struct gfx::UpdateUniqueId<MeshTreeDrawable>;

public:
  std::vector<Ptr> children;
  std::vector<MeshDrawable::Ptr> drawables;
  float4x4 transform = linalg::identity;
  std::string label;

  DrawablePtr clone() const override;
  IDrawableProcessor &getProcessor() const override;

  template <typename T> static inline void foreach (Ptr &item, T && callback) {
    callback(item);
    for (auto &child : item->children) {
      MeshTreeDrawable::foreach (child, callback);
    }
  }

  UniqueId getId() const override { return id; }

  void pipelineHashCollect(PipelineHashCollector &PipelineHashCollector) const override { assert(false); }

  // DrawableGeneration generation;
  // DrawableGeneration getGeneration() const override { return generation; }
};
} // namespace gfx

#endif /* D4C41EFB_5953_4831_9CB1_B6CCD9650575 */
