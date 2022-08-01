#ifndef B44846E1_9E91_4350_8C77_98F3A10E8882
#define B44846E1_9E91_4350_8C77_98F3A10E8882

#include <SDL_events.h>
#include <vector>

namespace shards::input {

struct IConsumer;
struct InputBuffer {
  typedef std::vector<SDL_Event> Events;
  typedef std::vector<IConsumer *> ConsumedBy;

private:
  Events events;
  ConsumedBy consumedBy;

public:
  struct Iterator {
    Events *events;
    ConsumedBy *consumedBy;
    bool onlyNonConsumed;
    size_t index;

    Iterator(Events &events, ConsumedBy &consumedBy, bool onlyNonConsumed, size_t index)
        : events(&events), consumedBy(&consumedBy), onlyNonConsumed(onlyNonConsumed), index(index) {
      if (onlyNonConsumed && getConsumedBy() != nullptr) {
        nextNonConsumed();
      }
    }
    operator bool() const { return index < events->size(); }
    Iterator &operator++() {
      nextNonConsumed();
      return *this;
    }
    bool operator==(const Iterator &other) const { return events == other.events && index == other.index; }
    bool operator!=(const Iterator &other) const { return events != other.events || index != other.index; }
    const SDL_Event *operator*() const { return &(*events)[index]; }
    const SDL_Event *operator->() const { return &(*events)[index]; }

    void consume(IConsumer *by) { (*consumedBy)[index] = by; }
    IConsumer *getConsumedBy() const { return (*consumedBy)[index]; }
    void nextNonConsumed() {
      do {
        ++index;
      } while (index < events->size() && (*consumedBy)[index] != nullptr);
    }
  };

  auto begin(bool onlyNonConsumed = true) { return Iterator(events, consumedBy, onlyNonConsumed, 0); }
  auto end() { return Iterator(events, consumedBy, false, events.size()); }

  size_t size() const { return events.size(); }
  const SDL_Event &operator[](const size_t i) const { return events[i]; }
  IConsumer *getConsumedBy(size_t i) const { return consumedBy[i]; }

  void clear() {
    events.clear();
    consumedBy.clear();
  }

  void push_back(const SDL_Event &event) {
    events.push_back(event);
    consumedBy.push_back(nullptr);
  }
};

struct IConsumer {
  virtual ~IConsumer() = default;
  virtual void handleInput(InputBuffer &inputBuffer) = 0;
};

struct Context {
private:
  struct Consumer {
    IConsumer *consumer;
    int priority;
  };
  std::vector<Consumer> consumers;

  static auto sort(const Consumer &a, const Consumer &b) { return a.priority > b.priority; }

public:
  void add(IConsumer *consumer, int priority) {
    Consumer item{.consumer = consumer, .priority = priority};
    auto it = std::lower_bound(consumers.begin(), consumers.end(), item, &sort);
    consumers.insert(it, item);
  }

  void remove(IConsumer *consumer) {
    consumers.erase(
        std::remove_if(consumers.begin(), consumers.end(), [&](const Consumer &a) { return a.consumer == consumer; }));
  }

  void dispatch(InputBuffer &buffer) {
    for (auto &consumer : consumers) {
      consumer.consumer->handleInput(buffer);
    }
  }
};

} // namespace shards::input

#endif /* B44846E1_9E91_4350_8C77_98F3A10E8882 */
