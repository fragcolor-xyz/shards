#define CATCH_CONFIG_RUNNER
#include <catch2/catch_all.hpp>
#include <spdlog/spdlog.h>
#include <input/input.hpp>
#include <linalg.h>
#include <stdint.h>

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
  strncpy_s(textEvent.text.text, "w", 1);
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
