#ifndef D4C41EFB_5953_4831_9CB1_B6CCD9650575
#define D4C41EFB_5953_4831_9CB1_B6CCD9650575

#include "../linalg.hpp"
#include "mesh_drawable.hpp"
#include <boost/container/small_vector.hpp>
#include <cassert>
#include <memory>
#include <set>

namespace gfx {

struct TRS {
  float3 translation;
  float3 scale = float3(1.0f);
  float4 rotation = float4(0.0f, 0.0f, 0.0f, 1.0f);

  TRS() = default;
  TRS(const float4x4 &other) { *this = other; }
  TRS &operator=(const TRS &other) = default;
  TRS &operator=(const float4x4 &other) {
    float3x3 rotationMatrix;
    decomposeTRS(other, translation, scale, rotationMatrix);
    this->rotation = linalg::rotation_quat(rotationMatrix);
    return *this;
  }

  float4x4 getMatrix() const {
    float4x4 rot = linalg::rotation_matrix(this->rotation);
    float4x4 scale = linalg::scaling_matrix(this->scale);
    float4x4 tsl = linalg::translation_matrix(this->translation);
    return linalg::mul(tsl, linalg::mul(rot, scale));
  }
};

// Transform tree of drawable objects
struct MeshTreeDrawable final : public IDrawable, public std::enable_shared_from_this<MeshTreeDrawable> {
  typedef std::shared_ptr<MeshTreeDrawable> Ptr;

private:
  UniqueId id = getNextDrawableId();
  friend struct gfx::UpdateUniqueId<MeshTreeDrawable>;
  std::vector<Ptr> children;

public:
  std::vector<MeshDrawable::Ptr> drawables;
  TRS trs;
  Ptr::weak_type parent;
  std::string label;

  DrawablePtr clone() const override;
  DrawableProcessorConstructor getProcessor() const override;

  // Visits each MeshTreeDrawable in this tree and calls callback on it
  template <typename T> static inline void foreach (const Ptr &item, T && callback);
  template <typename T> static inline void traverseUp(const Ptr &item, T &&callback, Ptr stopAt = Ptr());
  template <typename T> static inline void traverseDown(const Ptr &from, const Ptr &until, T &&callback);
  static inline Ptr findRoot(const Ptr &item);

  void setChildren(std::vector<Ptr> &&children) {
    for (auto &child : children) {
      child->parent = weak_from_this();
    }
    this->children = std::move(children);
  }

  void addChild(Ptr child) {
    children.push_back(child);
    child->parent = weak_from_this();
  }
  const std::vector<Ptr> &getChildren() const { return children; }

  UniqueId getId() const override { return id; }

  void pipelineHashCollect(detail::PipelineHashCollector &pipelineHashCollector) const override { assert(false); }

  bool expand(shards::pmr::vector<const IDrawable *> &outDrawables) const override;
};

template <typename T> void MeshTreeDrawable::foreach (const Ptr &item, T && callback) {
  struct Node {
    Ptr node;
  };
  boost::container::small_vector<Node, 16> queue{{item}};

  while (!queue.empty()) {
    Node node = queue.back();
    queue.pop_back();

    callback(node.node);

    for (auto &child : node.node->children) {
      queue.push_back(Node{child});
    }
  }
}

template <typename T> void MeshTreeDrawable::traverseUp(const Ptr &item, T &&callback, Ptr stopAt) {
  Ptr node = item;
  while (node && node != stopAt) {
    callback(node);
    node = node->parent.lock();
  }
}

template <typename T> void MeshTreeDrawable::traverseDown(const Ptr &from, const Ptr &until, T &&callback) {
  boost::container::small_vector<Ptr, 16> queue;
  Ptr node = until;
  while (node && node != from) {
    queue.push_back(node);
    node = node->parent.lock();
  }
  for (auto it = queue.rbegin(); it != queue.rend(); it++) {
    callback(*it);
  }
}

MeshTreeDrawable::Ptr MeshTreeDrawable::findRoot(const Ptr &item) {
  Ptr root;
  Ptr node = item;
  while (node) {
    root = node;
    node = node->parent.lock();
  }
  return root;
}

} // namespace gfx

#endif /* D4C41EFB_5953_4831_9CB1_B6CCD9650575 */
