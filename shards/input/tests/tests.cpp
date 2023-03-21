#define CATCH_CONFIG_RUNNER
#include <catch2/catch_all.hpp>
#include <spdlog/spdlog.h>
#include <input/input.hpp>
#include <input/detached.hpp>
#include <input/event_buffer.hpp>
#include <input/master.hpp>
#include <linalg.h>
#include <stdint.h>
#include <chrono>
#include <thread>
#include <future>

using namespace linalg::aliases;
using namespace shards::input;

struct RegionConsumer : public IConsumer {
  int2 min;
  int2 max;
  size_t receivedEventCount = 0;
  size_t consumedEventCount = 0;

  RegionConsumer(int2 min, int2 max) : min(min), max(max) {}
  bool isWithinBounds(int x, int y) { return (x >= min.x && x <= max.x) && (y >= min.y && y <= max.y); }
  void handleInput(InputBuffer &inputBuffer) override {
    for (auto it = inputBuffer.begin(); it; ++it) {
      ++receivedEventCount;
      switch (it->type) {
      case SDL_MOUSEMOTION: {
        auto &ievent = it->motion;
        if (isWithinBounds(ievent.x, ievent.y)) {
          it.consume(this);
          consumedEventCount++;
        }
        break;
      }
      case SDL_MOUSEBUTTONDOWN:
      case SDL_MOUSEBUTTONUP: {
        auto &ievent = it->button;
        if (isWithinBounds(ievent.x, ievent.y)) {
          it.consume(this);
          consumedEventCount++;
        }
        break;
      }
      }
    }
  }

  void reset() {
    receivedEventCount = 0;
    consumedEventCount = 0;
  }
};

void generateTestInputMotionUpDown(InputBuffer &buffer, int x, int y, uint8_t button) {
  buffer.push_back(SDL_Event{
      .motion = {.type = SDL_MOUSEMOTION, .x = x, .y = y},
  });
  buffer.push_back(SDL_Event{
      .button = {.type = SDL_MOUSEBUTTONDOWN, .button = button, .x = x, .y = y},
  });
  buffer.push_back(SDL_Event{
      .button = {.type = SDL_MOUSEBUTTONUP, .button = button, .x = x, .y = y},
  });
}

void generateKeyEvents(InputBuffer &buffer) {
  SDL_Event textEvent{
      .text = {.type = SDL_TEXTINPUT},
  };
  strncpy(textEvent.text.text, "w", 1);
  buffer.push_back(textEvent);

  buffer.push_back(SDL_Event{
      .key = {.type = SDL_KEYDOWN, .keysym = {.sym = SDLK_w}},
  });

  buffer.push_back(SDL_Event{
      .key = {.type = SDL_KEYUP, .keysym = {.sym = SDLK_w}},
  });
}

TEST_CASE("Mouse events") {
  RegionConsumer ra(int2(0, 0), int2(100, 100));
  RegionConsumer rb(int2(10, 10), int2(150, 150));
  auto resetConsumers = [&]() {
    ra.reset();
    rb.reset();
  };

  auto testCase = [&](Context &ctx) {
    InputBuffer inputs;
    generateTestInputMotionUpDown(inputs, 5, 0, SDL_BUTTON_LEFT);
    CHECK(inputs.size() == 3);

    resetConsumers();
    ctx.dispatch(inputs);

    CHECK(ra.receivedEventCount == 3);
    CHECK(rb.receivedEventCount == 0);
    CHECK(ra.consumedEventCount == 3);
    CHECK(rb.consumedEventCount == 0);

    CHECK(inputs.getConsumedBy(0) == &ra);
    CHECK(inputs.getConsumedBy(1) == &ra);
    CHECK(inputs.getConsumedBy(2) == &ra);

    inputs.clear();
    CHECK(inputs.size() == 0);

    generateTestInputMotionUpDown(inputs, 10, 10, SDL_BUTTON_RIGHT);

    resetConsumers();
    ctx.dispatch(inputs);

    CHECK(ra.receivedEventCount == 3);
    CHECK(rb.receivedEventCount == 0);
    CHECK(ra.consumedEventCount == 3);
    CHECK(rb.consumedEventCount == 0);

    CHECK(inputs.getConsumedBy(0) == &ra);
    CHECK(inputs.getConsumedBy(1) == &ra);
    CHECK(inputs.getConsumedBy(2) == &ra);

    inputs.clear();
    generateTestInputMotionUpDown(inputs, 101, 101, SDL_BUTTON_RIGHT);

    resetConsumers();
    ctx.dispatch(inputs);

    // Since ra has higher priority it still receives the events first
    CHECK(ra.receivedEventCount == 3);
    CHECK(rb.receivedEventCount == 3);
    CHECK(ra.consumedEventCount == 0);
    CHECK(rb.consumedEventCount == 3);

    CHECK(inputs.getConsumedBy(0) == &rb);
    CHECK(inputs.getConsumedBy(1) == &rb);
    CHECK(inputs.getConsumedBy(2) == &rb);
  };

  {
    Context ctx;
    ctx.add(&rb, 0);
    ctx.add(&ra, 1);
    testCase(ctx);
  }

  {
    Context ctx;
    ctx.add(&ra, 1);
    ctx.add(&rb, 0);
    testCase(ctx);
  }
}

TEST_CASE("Empty iterator") {
  size_t iterations = 0;
  InputBuffer buffer;
  for (auto it = buffer.begin(); it; ++it)
    iterations++;

  CHECK(iterations == 0);
}

TEST_CASE("Consume all key events") {
  RegionConsumer consumer(int2(0, 0), int2(100, 100));

  InputBuffer buffer;
  generateKeyEvents(buffer);
  CHECK(buffer.size() > 0);

  buffer.consumeEvents(ConsumeEventFilter::Keyboard);

  consumer.handleInput(buffer);
  CHECK(consumer.receivedEventCount == 0);
}

TEST_CASE("Consume all mouse events") {
  RegionConsumer consumer(int2(0, 0), int2(100, 100));

  InputBuffer buffer;
  generateTestInputMotionUpDown(buffer, 0, 0, 0);
  CHECK(buffer.size() > 0);

  buffer.consumeEvents(ConsumeEventFilter::Touch | ConsumeEventFilter::Mouse);

  consumer.handleInput(buffer);
  CHECK(consumer.receivedEventCount == 0);
}

TEST_CASE("DetachedInput state") {
  InputState state;
  DetachedInput detached;

  SECTION("Key events") {
    state.heldKeys.insert(SDLK_w);
    detached.update(state);
    {
      CHECK(detached.virtualInputEvents.size() == 1);
      auto *evt = std::get_if<KeyEvent>(&detached.virtualInputEvents[0]);
      CHECK(evt);
      CHECK(evt->key == SDLK_w);
      CHECK(evt->pressed == true);
    }

    detached.update(state);
    CHECK(detached.virtualInputEvents.size() == 0);

    state.heldKeys.clear();
    detached.update(state);
    {
      CHECK(detached.virtualInputEvents.size() == 1);
      auto *keyEvt = std::get_if<KeyEvent>(&detached.virtualInputEvents[0]);
      CHECK(keyEvt);
      CHECK(keyEvt->key == SDLK_w);
      CHECK(keyEvt->pressed == false);
    }
  }

  SECTION("Mouse buttons") {
    state.mouseButtonState = SDL_BUTTON(SDL_BUTTON_LEFT);
    detached.update(state);
    {
      CHECK(detached.virtualInputEvents.size() == 1);
      auto *evt = std::get_if<PointerButtonEvent>(&detached.virtualInputEvents[0]);
      CHECK(evt);
      CHECK(evt->index == SDL_BUTTON_LEFT);
      CHECK(evt->pressed == true);
    }

    detached.update(state);
    CHECK(detached.virtualInputEvents.size() == 0);

    state.mouseButtonState = 0;
    detached.update(state);
    {
      CHECK(detached.virtualInputEvents.size() == 1);
      auto *evt = std::get_if<PointerButtonEvent>(&detached.virtualInputEvents[0]);
      CHECK(evt);
      CHECK(evt->index == SDL_BUTTON_LEFT);
      CHECK(evt->pressed == false);
    }
  }

  SECTION("Scroll") {
    state.reset();
    SDL_Event sdlEvt{};
    sdlEvt.wheel.preciseY = 1.0f;
    sdlEvt.wheel.type = SDL_MOUSEWHEEL;
    detached.apply(sdlEvt);
    detached.update(state);
    {
      CHECK(detached.virtualInputEvents.size() == 1);
      auto *evt = std::get_if<ScrollEvent>(&detached.virtualInputEvents[0]);
      CHECK(evt);
      CHECK(evt->delta == 1.0f);
    }

    detached.update(state);
    CHECK(detached.virtualInputEvents.size() == 0);
    detached.update(state);
    CHECK(detached.virtualInputEvents.size() == 0);

    sdlEvt.wheel.preciseY = 0.2f;
    detached.apply(sdlEvt);

    sdlEvt.wheel.preciseY = -0.7f;
    detached.apply(sdlEvt);

    detached.update(state);
    {
      CHECK(detached.virtualInputEvents.size() == 1);
      auto *evt = std::get_if<ScrollEvent>(&detached.virtualInputEvents[0]);
      CHECK(evt);
      CHECK(evt->delta == -0.5f);
    }
  }

  SECTION("Pointer Move") {
    state.cursorPosition = int2{20, 1};
    detached.update(state);
    {
      CHECK(detached.virtualInputEvents.size() == 1);
      auto *evt = std::get_if<PointerMoveEvent>(&detached.virtualInputEvents[0]);
      CHECK(evt);
      CHECK(evt->delta == state.cursorPosition);
      CHECK(evt->pos == state.cursorPosition);
    }

    detached.update(state);
    CHECK(detached.virtualInputEvents.size() == 0);

    state.cursorPosition = int2{21, 3};
    detached.update(state);
    {
      CHECK(detached.virtualInputEvents.size() == 1);
      auto *evt = std::get_if<PointerMoveEvent>(&detached.virtualInputEvents[0]);
      CHECK(evt);
      CHECK(evt->delta == int2{1, 2});
      CHECK(evt->pos == state.cursorPosition);
    }

    detached.update(state);
    CHECK(detached.virtualInputEvents.size() == 0);
  }
}

size_t numDummyEvents(size_t index) { return index % 3 + 1; }

Event produceDummyEvent(size_t index) {
  switch (index % 3) {
  case 0:
    return PointerButtonEvent{.index = SDL_BUTTON_LEFT, .pressed = true};
  case 1:
    return PointerMoveEvent{.pos = int2{1 + float(index % 10), 2 + float(index % 2)}, .delta = int2{3, 4}};
  case 2:
    return KeyEvent{.key = SDL_Keycode(SDLK_a + (index % 20)), .pressed = true};
  }
  throw std::logic_error("Invalid index");
}

void threadedTestCase(size_t numProducerFrames, std::chrono::milliseconds sleepProducer, size_t numConsumerFrames,
                      std::chrono::milliseconds sleepConsumer, bool checkLossless = true) {
  using namespace std::chrono_literals;
  std::vector<Event> expectedAllEvents;
  for (auto i = 0; i < numProducerFrames; i++) {
    for (auto j = 0; j < numDummyEvents(i); j++) {
      expectedAllEvents.push_back(produceDummyEvent(i + j));
    }
  }

  EventBuffer<> buffer;
  auto producer = std::async([&]() {
    for (size_t i = 0; i < numProducerFrames; i++) {
      auto &nextFrame = buffer.getNextFrame();
      for (size_t j = 0; j < numDummyEvents(i); j++)
        nextFrame.events.push_back(produceDummyEvent(i + j));
      buffer.nextFrame();
      std::this_thread::sleep_for(sleepProducer);
    }
  });

  auto consumer = std::async([&]() {
    std::vector<Event> allEvents;

    size_t lastGeneration = 0;
    for (size_t iteration = 0; iteration < numConsumerFrames; iteration++) {
      auto pair = buffer.getEvents(lastGeneration);
      if (!pair.frames.empty()) {
        spdlog::debug("Iteration {}, Got {} frame", iteration, pair.frames.size());
        for (auto &[frameIndex, frame] : pair.frames) {
          auto &events = frame.events;
          spdlog::debug(" Frame {}: {} events", frameIndex, events.size());
          CHECK(events.size() == numDummyEvents(frameIndex));
          for (size_t i = 0; i < events.size(); i++) {
            CHECK(events[i] == produceDummyEvent(frameIndex + i));
            allEvents.push_back(events[i]);
          }
        }
        lastGeneration = pair.lastGeneration;
      }
      std::this_thread::sleep_for(sleepConsumer);
    }
    CHECK(lastGeneration == numProducerFrames);

    if (checkLossless) {
      CHECK(allEvents.size() == expectedAllEvents.size());
      for (size_t i = 0; i < allEvents.size(); i++) {
        CHECK(allEvents[i] == expectedAllEvents[i]);
      }
    }
  });

  producer.wait();
  consumer.wait();
}

using namespace std::chrono_literals;
TEST_CASE("EventBuffer 1") { threadedTestCase(64, 10ms, 80, 18ms); }
TEST_CASE("EventBuffer 2") { threadedTestCase(64, 10ms, 80, 26ms); }
TEST_CASE("EventBuffer 3") { threadedTestCase(64, 1ms, 80, 26ms); }
TEST_CASE("EventBuffer Ring Wrapping") { threadedTestCase(1024, 1ms, 1400, 1ms); }

TEST_CASE("Input consumption") {
  // EventBuffer<ConsumableEvent> mainEventBuffer;

  // std::vector<std::function<void(Frame &)>> handlers;
}

int main(int argc, char *argv[]) {
  Catch::Session session;

  spdlog::set_level(spdlog::level::debug);

  int returnCode = session.applyCommandLine(argc, argv);
  if (returnCode != 0) // Indicates a command line error
    return returnCode;

  auto &configData = session.configData();
  (void)configData;

  int result = session.run();

  return result;
}
