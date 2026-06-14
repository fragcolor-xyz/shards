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

TEST_CASE("DetachedInput state") {
  InputState state;
  DetachedInput detached;

  SECTION("Key events") {
    // Key-down events arrive as native SDL key events and are decoded into a
    // KeyEvent (down events are not synthesized from held-key diffs).
    detached.updateNative([](auto apply) {
      SDL_Event sdlEvt{};
      sdlEvt.type = SDL_EVENT_KEY_DOWN;
      sdlEvt.key.type = SDL_EVENT_KEY_DOWN;
      sdlEvt.key.key = SDLK_W;
      sdlEvt.key.repeat = false;
      apply(sdlEvt);
    });
    {
      CHECK(detached.virtualInputEvents.size() == 1);
      auto *evt = std::get_if<KeyEvent>(&detached.virtualInputEvents[0]);
      REQUIRE(evt);
      CHECK(evt->key == SDLK_W);
      CHECK(evt->pressed == true);
    }

    // No change in held keys -> no events.
    detached.update(state);
    CHECK(detached.virtualInputEvents.size() == 0);

    // Key-up is synthesized by diffing the held-key state: mark W as held, then
    // update with it released.
    detached.state.heldKeys.insert(SDLK_W);
    InputState released;
    detached.update(released);
    {
      CHECK(detached.virtualInputEvents.size() == 1);
      auto *keyEvt = std::get_if<KeyEvent>(&detached.virtualInputEvents[0]);
      REQUIRE(keyEvt);
      CHECK(keyEvt->key == SDLK_W);
      CHECK(keyEvt->pressed == false);
    }
  }

  SECTION("Mouse buttons") {
    state.mouseButtonState = SDL_BUTTON_MASK(SDL_BUTTON_LEFT);
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

    // Native (SDL) events are fed through the public updateNative() entry point,
    // which decodes them and synthesizes the corresponding virtual events.
    detached.updateNative([](auto apply) {
      SDL_Event sdlEvt{};
      sdlEvt.wheel.type = SDL_EVENT_MOUSE_WHEEL;
      sdlEvt.wheel.y = 1.0f;
      apply(sdlEvt);
    });
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

    // Multiple wheel events within a single update accumulate into one ScrollEvent.
    detached.updateNative([](auto apply) {
      SDL_Event sdlEvt{};
      sdlEvt.wheel.type = SDL_EVENT_MOUSE_WHEEL;
      sdlEvt.wheel.y = 0.2f;
      apply(sdlEvt);
      sdlEvt.wheel.y = -0.7f;
      apply(sdlEvt);
    });
    {
      CHECK(detached.virtualInputEvents.size() == 1);
      auto *evt = std::get_if<ScrollEvent>(&detached.virtualInputEvents[0]);
      CHECK(evt);
      CHECK(evt->delta == -0.5f);
    }
  }

  SECTION("Pointer Move") {
    state.cursorPosition = float2{20, 1};
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

    state.cursorPosition = float2{21, 3};
    detached.update(state);
    {
      CHECK(detached.virtualInputEvents.size() == 1);
      auto *evt = std::get_if<PointerMoveEvent>(&detached.virtualInputEvents[0]);
      CHECK(evt);
      CHECK(evt->delta == float2{1, 2});
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
    return PointerMoveEvent{.pos = float2{1 + float(index % 10), 2 + float(index % 2)}, .delta = float2{3, 4}};
  case 2:
    return KeyEvent{.key = SDL_Keycode(SDLK_A + (index % 20)), .pressed = true};
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

int main(int argc, char *argv[]) {
  Catch::Session session;

  int returnCode = session.applyCommandLine(argc, argv);
  if (returnCode != 0) // Indicates a command line error
    return returnCode;

  auto &configData = session.configData();
  (void)configData;

#if SHARDS_GFX_SDL
  // DetachedInput::updateNative() polls SDL keyboard/mod state; initialize the
  // events subsystem so that is well-defined (and empty) in headless CI. Events
  // does not require a display, unlike video.
  SDL_InitSubSystem(SDL_INIT_EVENTS);
#endif

  int result = session.run();

#if SHARDS_GFX_SDL
  SDL_QuitSubSystem(SDL_INIT_EVENTS);
#endif

  return result;
}
