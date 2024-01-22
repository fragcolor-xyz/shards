#ifndef F07CDE82_F36C_4A25_A28F_7A44EA5E27B4
#define F07CDE82_F36C_4A25_A28F_7A44EA5E27B4

#include "bounds.hpp"
#include "log.hpp"
#include "drawable.hpp"
// #include "culling.hpp"
#include <boost/container/flat_map.hpp>
#include <boost/core/span.hpp>

namespace gfx::detail {

inline shards::logging::Logger getQueueLogger() {
  return shards::logging::getOrCreate("gfx_queue", [](shards::logging::Logger logger) {
    logger->set_level(spdlog::level::info);
  });
}

struct QueueData {
  struct Entry {
    size_t drawableVersion = size_t(~0);
    size_t queueVersion;
    OrientedBounds bounds;
  };

  UniqueId queueId;
  bool trace{};
  size_t lastTouched{}; // Usage of the queue itself

  // Version of the queue usage
  size_t queueVersion{};

  // The set of drawables with mapped persistent state
  boost::container::flat_map<UniqueId, Entry> set;

  QueueData(DrawQueuePtr queue) : queueId(queue->getId()) { trace = queue->trace; }

  inline void updateEntry(Entry &entry, const IDrawable *item) {
    if (trace)
      SPDLOG_LOGGER_TRACE(getQueueLogger(), "Update drawable {} in queue {}", item->getId().getIdPart(), queueId.getIdPart());
    entry.bounds = item->getBounds();
  }

  inline void update(boost::span<const IDrawable *> drawables) {
    ++queueVersion;

    for (auto &item : drawables) {
      auto itemId = item->getId();
      auto it = set.find(itemId);
      if (it == set.end()) {
        if (trace)
          SPDLOG_LOGGER_TRACE(getQueueLogger(), "Add drawable {} to queue {}", itemId.getIdPart(), queueId.getIdPart());
        it = set.emplace(itemId, Entry{}).first;
      }
      auto &entry = it->second;
      entry.queueVersion = queueVersion;
      if (entry.drawableVersion != item->getVersion()) {
        entry.drawableVersion = item->getVersion();
        updateEntry(entry, item);
      }
    }

    for (auto it = set.begin(); it != set.end();) {
      auto &entry = it->second;
      if (entry.queueVersion != queueVersion) {
        if (trace)
          SPDLOG_LOGGER_TRACE(getQueueLogger(), "Remove drawable {} from queue {}", it->first.getIdPart(), queueId.getIdPart());
        it = set.erase(it);
      } else {
        ++it;
      }
    }
  }
};
using QueueDataPtr = std::shared_ptr<QueueData>;
} // namespace gfx::detail

#endif /* F07CDE82_F36C_4A25_A28F_7A44EA5E27B4 */
