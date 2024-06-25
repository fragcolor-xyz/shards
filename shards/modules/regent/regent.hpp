#ifndef B989AD66_38FC_43EB_A594_6ECAE8D92E28
#define B989AD66_38FC_43EB_A594_6ECAE8D92E28

#include <memory>
#include <linalg.h>
#include <boost/container/flat_map.hpp>
#include <boost/container/stable_vector.hpp>
#include <shards/core/assert.hpp>
#include <vector>
#include <atomic>

#include <tracy/Wrapper.hpp>

namespace shards::regent {
using namespace linalg::aliases;

struct Node {
  static std::atomic_uint64_t UidCounter;
  uint64_t uid = UidCounter++;

  // The world transform of this node
  float4x4 transform;
  // Persist this node even if not touched during a frame
  bool persistence : 1 = false;
  // Can be used to toggle this node even if it has persistence
  bool enabled : 1 = true;

  // About the size of MeshDrawable, placeholder
  uint8_t someData[336];
};

struct Core : public std::enable_shared_from_this<Core> {
  struct AssociatedData {
    std::shared_ptr<Node> node;
    AssociatedData *next{};
    size_t lastTouched{};
  };
  using MapType = boost::container::flat_map<Node *, AssociatedData, std::less<Node *>, //
                                             boost::container::stable_vector<std::pair<Node *, AssociatedData>>>;

private:
  static std::atomic_uint64_t UidCounter;
  uint64_t uid = UidCounter++;

  MapType nodeMap;
  std::vector<AssociatedData *> activeNodes;
  // std::vector<AssociatedData *> removedNodes;
  size_t frameCounter_{};

  // Optimized iteration state
  // AssociatedData *firstNode{};
  // AssociatedData *iterator{};

public:
  void begin() {
    ++frameCounter_;
    activeNodes.clear();
    // removedNodes.clear();
  }

  uint64_t getId() const { return uid; }

  const MapType &getNodeMap() const { return nodeMap; }
  const auto &getActiveNodes() const { return activeNodes; }

  size_t frameCounter() const { return frameCounter_; }

  std::shared_ptr<Node> createNewNode() {
    auto node = std::make_shared<Node>();
    addNodeInternal(node);
    return node;
  }

  void touchNode(const std::shared_ptr<Node> &node) {
    ZoneScopedN("Regent::touch");

    auto it = nodeMap.find(node.get());
    shassert(it != nodeMap.end() && "Node not found");

    auto &nodeData = it->second;
    if (nodeData.lastTouched != frameCounter_) {
      nodeData.lastTouched = frameCounter_;
      if (node->enabled)
        activeNodes.push_back(&nodeData);
    }

    // Invalidate iterator
    // iterator = nullptr;
  }

  void end() {
    ZoneScopedN("Regent::end");
    for (auto it = nodeMap.begin(); it != nodeMap.end(); ++it) {
      if (it->second.lastTouched != frameCounter_) {
        if (it->first->persistence && it->first->enabled) {
          activeNodes.push_back(&it->second);
        }
      }
    }
  }

private:
  void addNodeInternal(const std::shared_ptr<Node> &node) {
    ZoneScopedN("Regent::addNode");
    Node *key = node.get();
    auto it = nodeMap.emplace(key, AssociatedData{node}).first;
    // addedNodes.push_back(&it->second);
  }
  void removeNodeInternal(const std::shared_ptr<Node> &node) {
    // auto it = nodes.find(node.get());
    // if (it != nodes.end()) {
    //   removedNodes.push_back(it->second);
    //   nodes.erase(it);
    // }
  }
};

template <typename T> struct ManyContainer {
private:
  boost::container::stable_vector<T> data;
  size_t allocIndex{};
  size_t version = size_t(-1);

public:
  template <typename F> T &pull(size_t version_, F &&makeNew = []() { return T(); }) {
    if (version != version_) {
      reset(version_);
    }
    if (allocIndex >= data.size()) {
      data.push_back(makeNew());
    }
    return data[allocIndex++];
  }

  void clear() {
    data.clear();
    allocIndex = 0;
  }

private:
  void reset(size_t version_) {
    allocIndex = 0;
    version = version_;
  }
};

} // namespace shards::regent

#endif /* B989AD66_38FC_43EB_A594_6ECAE8D92E28 */
