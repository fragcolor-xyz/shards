#ifndef GFX_GRAPH
#define GFX_GRAPH

#include "../core/assert.hpp"
#include <deque>
#include <set>
#include <stdint.h>
#include <vector>

namespace graph {

struct Node {
  std::vector<size_t> dependencies;
};

struct Graph {
  std::vector<Node> nodes;
};

struct TopologicalSort {
  std::deque<size_t> queue;
  std::vector<std::set<size_t>> incomingEdgesPerNode;
  std::vector<std::set<size_t>> outgoingEdgesPerNode;

  // returns true if able to sort the graph, false if the graph contains cycles
  inline bool operator()(const Graph &graph, std::vector<size_t> &sorted) {
    size_t numNodes = graph.nodes.size();
    incomingEdgesPerNode.resize(numNodes);
    outgoingEdgesPerNode.resize(numNodes);

    for (size_t i = 0; i < numNodes; i++) {
      for (auto &dep : graph.nodes[i].dependencies) {
        incomingEdgesPerNode[i].insert(dep);
        outgoingEdgesPerNode[dep].insert(i);
      }
    }

    return apply(graph, sorted);
  }

private:
  inline bool apply(const Graph &graph, std::vector<size_t> &sorted) {
    // Implement Kahn's algorithm
    size_t numNodes = graph.nodes.size();

    if (numNodes == 0)
      return true;

    shassert(incomingEdgesPerNode.size() == numNodes);
    shassert(outgoingEdgesPerNode.size() == numNodes);

    queue.clear();
    for (size_t i = 0; i < numNodes; i++) {
      auto &edges = incomingEdgesPerNode[i];
      if (edges.size() == 0) {
        queue.push_back(i);
      }
    }

    if (queue.empty()) {
      return false; // graph is cyclical
    }

    sorted.clear();
    while (!queue.empty()) {
      size_t index = queue.front();
      queue.pop_front();

      sorted.push_back(index);
      for (auto &otherIndex : outgoingEdgesPerNode[index]) {
        incomingEdgesPerNode[otherIndex].erase(index);
        if (incomingEdgesPerNode[otherIndex].empty()) {
          queue.push_back(otherIndex);
        }
      }
    }

    for (auto &edges : incomingEdgesPerNode) {
      if (!edges.empty())
        return false; // Graph has remaining cycles
    }

    return true;
  }
};

} // namespace graph

#endif // GFX_GRAPH
