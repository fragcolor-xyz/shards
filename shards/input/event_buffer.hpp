#ifndef D318F97B_C186_4890_A4AB_6BF78F80B4CA
#define D318F97B_C186_4890_A4AB_6BF78F80B4CA

#include "events.hpp"
#include "state.hpp"
#include <atomic>
#include <tuple>
#include <vector>
#include <type_traits>

namespace shards::input {

template <typename TEvent> struct Frame {
  std::vector<TEvent> events;
  InputState state;

  void clear() { events.clear(); }
};

// A circular buffer of events.
template <typename TFrame = Frame<Event>, size_t HistoryLength = 1024, size_t Deadzone = 8> struct EventBuffer {
  TFrame frames[HistoryLength];

  struct FrameIterator {
    size_t generation;
    size_t generationEnd;
    TFrame *ring{};

    const FrameIterator &operator++() {
      ++generation;
      return *this;
    }

    bool operator==(const FrameIterator &end) const {
      if (!*this && !end)
        return true;
      return ring == end.ring && generation == end.generation;
    }

    operator bool() const { return ring && generation < generationEnd; }

    const std::tuple<size_t, TFrame &> operator*() const { return {generation, std::ref(ring[generation % HistoryLength])}; }
  };

  struct Range {
    EventBuffer &buffer;
    size_t generation;
    size_t generationEnd;

    FrameIterator begin() { return buffer.getIterator(generation); }
    FrameIterator end() { return buffer.getIterator(generationEnd); }
    size_t size() const { return generationEnd - generation; }
    bool empty() const {
      auto &mutableSelf = *const_cast<Range *>(this);
      return mutableSelf.begin() == mutableSelf.end();
    }
  };

  // Points to the oldest available item in the buffer.
  std::atomic<size_t> tail{};

  // Points to the next item to be written.
  // (or) Points to one past the last written item.
  std::atomic<size_t> head{};

  size_t getFrameIndex(int offset) const { return size_t((int(head) + int(offset)) % HistoryLength); }

  TFrame &getFrame(int offset) { return frames[getFrameIndex(offset)]; }
  const TFrame &getFrame(int offset) const { return frames[getFrameIndex(offset)]; }

  const TFrame &getLastFrame() {
    assert(!empty());
    return getFrame(-1);
  }

  TFrame &getNextFrame() { return getFrame(0); }

  void nextFrame() {
    if ((head - tail) > (HistoryLength - Deadzone)) {
      tail = head - HistoryLength + Deadzone;
    }

    getFrame(1).clear();
    ++head;
  }

  bool empty() const { return head == tail; }

  FrameIterator getIterator(size_t generation) {
    size_t tail = this->tail;
    size_t head = this->head;
    if (generation > head)
      generation = head;
    if (generation < tail)
      generation = tail;
    return FrameIterator{generation, head, frames};
  }

  Range getEvents(size_t since, size_t until) {
    size_t head = this->head;
    size_t tail = this->tail;
    if (until > head)
      until = head;
    if (since < tail)
      since = tail;
    return Range{*this, since, until};
  }

  struct ResultUpToLatest {
    Range frames;
    size_t lastGeneration;
  };
  ResultUpToLatest getEvents(size_t since) {
    size_t head = this->head;
    return {getEvents(since, head), head};
  }
};
} // namespace shards::input

#endif /* D318F97B_C186_4890_A4AB_6BF78F80B4CA */
