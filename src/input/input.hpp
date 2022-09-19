#ifndef B44846E1_9E91_4350_8C77_98F3A10E8882
#define B44846E1_9E91_4350_8C77_98F3A10E8882

#include <SDL_events.h>
#include <vector>
#include <algorithm>

namespace shards::input {

enum class ConsumeEventFilter : uint8_t {
  None = 0,
  Keyboard = 1 << 0,
  PointerDown = 1 << 1,
  PointerUp = 1 << 2,
  Controller = 1 << 3,
};

ConsumeEventFilter categorizeEvent(const SDL_Event &event);

inline ConsumeEventFilter operator&(ConsumeEventFilter a, ConsumeEventFilter b) {
  return ConsumeEventFilter(uint8_t(a) & uint8_t(b));
}
inline ConsumeEventFilter operator|(ConsumeEventFilter a, ConsumeEventFilter b) {
  return ConsumeEventFilter(uint8_t(a) | uint8_t(b));
}

struct IConsumer;
struct InputBufferIterator;
struct InputBuffer {
  typedef std::vector<SDL_Event> Events;
  typedef std::vector<void *> ConsumedBy;
  friend struct InputBufferIterator;

private:
  Events events;
  ConsumedBy consumedBy;

public:
  InputBufferIterator begin(bool onlyNonConsumed = true);
  InputBufferIterator end();

  size_t size() const { return events.size(); }
  const SDL_Event &operator[](const size_t i) const { return events[i]; }
  operator const std::vector<SDL_Event> &() const { return events; }
  void *getConsumedBy(size_t i) const { return consumedBy[i]; }

  void consumeEvents(ConsumeEventFilter filter, void *by = (void *)1);

  void clear() {
    events.clear();
    consumedBy.clear();
  }

  void push_back(const SDL_Event &event) {
    events.push_back(event);
    consumedBy.push_back(nullptr);
  }
};

struct InputBufferIterator {
  InputBuffer *buffer{};
  bool onlyNonConsumed;
  size_t index{};

  InputBufferIterator() = default;
  InputBufferIterator(InputBuffer *buffer, bool onlyNonConsumed, size_t index)
      : buffer(buffer), onlyNonConsumed(onlyNonConsumed), index(index) {
    if (onlyNonConsumed && buffer->size() > 0 && getConsumedBy() != nullptr) {
      nextNonConsumed();
    }
  }
  operator bool() const { return buffer && index < buffer->events.size(); }
  InputBufferIterator &operator++() {
    nextNonConsumed();
    return *this;
  }
  bool operator==(const InputBufferIterator &other) const { return buffer == other.buffer && index == other.index; }
  bool operator!=(const InputBufferIterator &other) const { return buffer != other.buffer || index != other.index; }
  const SDL_Event &operator*() const { return buffer->events[index]; }
  const SDL_Event *operator->() const { return &buffer->events[index]; }

  void consume(void *by = (void *)1) { buffer->consumedBy[index] = by; }
  void *getConsumedBy() const { return buffer->consumedBy[index]; }
  void nextNonConsumed() {
    do {
      ++index;
    } while (index < buffer->events.size() && buffer->consumedBy[index] != nullptr);
  }
};

inline InputBufferIterator InputBuffer::begin(bool onlyNonConsumed) { return InputBufferIterator(this, onlyNonConsumed, 0); }
inline InputBufferIterator InputBuffer::end() { return InputBufferIterator(this, false, events.size()); }

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
