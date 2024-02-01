#ifndef D4C41EFB_5953_4831_9CB1_B6CCD9650575
#define D4C41EFB_5953_4831_9CB1_B6CCD9650575

#include "../linalg.hpp"
#include "mesh_drawable.hpp"
#include "../anim/path.hpp"
#include "../debug_visualize.hpp"
#include "../trs.hpp"
#include <boost/container/small_vector.hpp>
#include <cassert>
#include <memory>
#include <set>

namespace gfx {

// Transform tree of drawable objects
struct MeshTreeDrawable final : public IDrawable, public IDebugVisualize, public std::enable_shared_from_this<MeshTreeDrawable> {
  typedef std::shared_ptr<MeshTreeDrawable> Ptr;
  friend struct TransformUpdaterCollector;

  struct CloningContext {
    std::map<std::shared_ptr<Skin>, std::shared_ptr<Skin>> skinMap;
  };

private:
  UniqueId id = getNextDrawableId();
  friend struct gfx::UpdateUniqueId<MeshTreeDrawable>;
  std::vector<Ptr> children;
  size_t version{};
  size_t lastUpdateVersion = size_t(~0);

public:
  std::vector<MeshDrawable::Ptr> drawables;
  float4x4 resolvedTransform;
  TRS trs;
  Ptr::weak_type parent;
  FastString name;

  DrawablePtr clone() const override;
  DrawablePtr clone(CloningContext &ctx) const;

  DrawablePtr self() const override { return const_cast<MeshTreeDrawable *>(this)->shared_from_this(); }

  DrawableProcessorConstructor getProcessor() const override;

  size_t getVersion() const override { return version; }
  void update() { ++version; }

  // Visits each MeshTreeDrawable in this tree and calls callback on it
  template <typename T> static inline void foreach (const Ptr &item, T && callback);
  template <typename T> static inline void traverseUp(const Ptr &item, T &&callback, Ptr stopAt = Ptr());
  template <typename T> static inline void traverseDown(const Ptr &from, const Ptr &until, T &&callback);
  template <typename T> static inline bool traverseDepthFirst(const Ptr &from, T &&callback);
  static inline Ptr findRoot(const Ptr &item);

  anim::Path getPath(Ptr rootNode = nullptr) const {
    auto self = std::const_pointer_cast<MeshTreeDrawable>(shared_from_this());
    if (!rootNode)
      rootNode = findRoot(self);
    anim::Path path;
    MeshTreeDrawable::traverseDown(rootNode, self, [&](auto &node) { path.push_back(node->name); });
    return path;
  }

  Ptr findNode(FastString name) {
    Ptr found;
    traverseDepthFirst(shared_from_this(), [&](Ptr node) {
      if (node->name == name) {
        found = node;
        return false;
      }
      return true;
    });
    return found;
  }

  MeshTreeDrawable::Ptr resolvePath(const anim::Path &p_) const {
    anim::Path p = p_;
    return resolvePath(p);
  }

  // Modifies path and returns the mesh tree node found
  // the path will contain the elements after the node that was found
  // there can only be builtin target identifiers left in the path
  MeshTreeDrawable::Ptr resolvePath(anim::Path &p) const {
    auto node = shared_from_this();
    while (true) {
      if (!node)
        return MeshTreeDrawable::Ptr();

      // Stop when end of path is reached
      // or we encounter a builtin target identifier
      if (!p || anim::isGltfBuiltinTarget(p.getHead()))
        return std::const_pointer_cast<MeshTreeDrawable>(node);

      for (auto &child : node->getChildren()) {
        if (child->name == p.getHead()) {
          node = child;
          p = p.next();
          goto _next;
        }
      }
      return MeshTreeDrawable::Ptr();
    _next:;
    }
  }

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

  bool expand(shards::pmr::vector<const IDrawable *> &outDrawables, shards::pmr::PolymorphicAllocator<> alloc) const override;

  void debugVisualize(ShapeRenderer &renderer) override;
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

template <typename T> bool MeshTreeDrawable::traverseDepthFirst(const Ptr &from, T &&callback) {
  boost::container::small_vector<Ptr, 64> queue;
  Ptr node = from;
  for (auto &child : from->children)
    if (!traverseDepthFirst(child, callback))
      return false;

  return callback(from);
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
