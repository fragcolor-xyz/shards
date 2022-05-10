#ifndef GFX_GRAPH
#define GFX_GRAPH

#include <cassert>
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

bool topologicalSort(const Graph &graph, std::vector<std::set<size_t>> &incomingEdgesPerNode,
                     std::vector<std::set<size_t>> &outgoingEdgesPerNode, std::vector<size_t> &sorted) {
  // Implement Kahn's algorithm
  size_t numNodes = graph.nodes.size();

  if (numNodes == 0)
    return true;

  assert(incomingEdgesPerNode.size() == numNodes);
  assert(outgoingEdgesPerNode.size() == numNodes);

  std::deque<size_t> queue;
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

// returns true if able to sort the graph, false if the graph contains cycles
bool topologicalSort(const Graph &graph, std::vector<size_t> &sorted) {
  size_t numNodes = graph.nodes.size();
  std::vector<std::set<size_t>> incomingEdgesPerNode;
  std::vector<std::set<size_t>> outgoingEdgesPerNode;
  incomingEdgesPerNode.resize(numNodes);
  outgoingEdgesPerNode.resize(numNodes);

  for (size_t i = 0; i < numNodes; i++) {
    for (auto &dep : graph.nodes[i].dependencies) {
      incomingEdgesPerNode[i].insert(dep);
      outgoingEdgesPerNode[dep].insert(i);
    }
  }

  return topologicalSort(graph, incomingEdgesPerNode, outgoingEdgesPerNode, sorted);
}

} // namespace graph

#endif // GFX_GRAPH
