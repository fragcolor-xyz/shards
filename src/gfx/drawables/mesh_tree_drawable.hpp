#ifndef D4C41EFB_5953_4831_9CB1_B6CCD9650575
#define D4C41EFB_5953_4831_9CB1_B6CCD9650575

#include "mesh_drawable.hpp"
#include <cassert>
#include <set>

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
  DrawableProcessorConstructor getProcessor() const override;

  // Visits each MeshTreeDrawable in this tree and calls callback on it
  template <typename T> static inline void foreach (Ptr &item, T && callback);

  UniqueId getId() const override { return id; }

  void pipelineHashCollect(detail::PipelineHashCollector &pipelineHashCollector) const override { assert(false); }

  bool expand(shards::pmr::vector<const IDrawable *> &outDrawables) const override;
};

template <typename T> void MeshTreeDrawable::foreach (Ptr &item, T && callback) {
  struct Node {
    Ptr node;
  };
  std::vector<Node> queue{{item}};

  while (!queue.empty()) {
    Node node = queue.back();
    queue.pop_back();

    callback(node.node);

    for (auto &child : node.node->children) {
      queue.push_back(Node{child});
    }
  }
}

} // namespace gfx

#endif /* D4C41EFB_5953_4831_9CB1_B6CCD9650575 */
