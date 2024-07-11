#ifndef CA4EC896_C746_4AC6_A229_560CF01EEFEA
#define CA4EC896_C746_4AC6_A229_560CF01EEFEA

#include "egui_types.hpp"
#include <gfx/linalg.hpp>
#ifndef RUST_BINDGEN
#include <shards/input/events.hpp>
#include <shards/input/messages.hpp>
#endif
#include <deque>
#include <vector>

namespace gfx {
struct Window;
struct EguiInputTranslatorPrivate;

#ifndef RUST_BINDGEN
struct EguiInputTranslatorArgs {
  const std::vector<shards::input::ConsumableEvent> &events;
  double time;
  float deltaTime;
  // The sizes of the input surface
  shards::input::InputRegion region;
  // Have focus or no focus active
  bool canReceiveInput;
  // The sub-section of the physical size that is mapped to this UI
  int4 mappedWindowRegion;
};
#endif

/// <div rustbindgen opaque></div>
struct EguiInputTranslator {
private:
  egui::Input input;
  std::deque<std::string> strings;
  std::vector<egui::InputEvent> events;
  egui::Pos2 lastCursorPosition;
  bool imeComposing{};
  bool textInputActive{};
  float2 windowToEguiScale;
  float4 mappedWindowRegion;

#ifndef RUST_BINDGEN
  std::vector<shards::input::Message> outputMessages;
#endif

public:
  EguiInputTranslator() = default;
  EguiInputTranslator(const EguiInputTranslator &) = delete;
  const EguiInputTranslator &operator=(const EguiInputTranslator &) = delete;

#ifndef RUST_BINDGEN
  // Setup input mapping from a window to a specific subregion
  void setupInputRegion(const shards::input::InputRegion &region, const int4 &mappedWindowRegion);

  // Resets the conversion output
  void begin(double time, float deltaTime);
  // Takes the input event and return true when it was converted into an egui event
  bool translateEvent(const EguiInputTranslatorArgs &args, const shards::input::ConsumableEvent &event);
  // Finalizes the egui::Input result
  void end();

  // ALternative to calling the above 4 and returning the output
  const egui::Input *translateFromInputEvents(const EguiInputTranslatorArgs &args);

  const std::vector<shards::input::Message> &getOutputMessages() const { return outputMessages; }
#endif

  const std::vector<egui::InputEvent> &getTranslatedEvents() const { return events; }

  // Manually push an event into the result
  void pushEvent(const egui::InputEvent &event) { events.push_back(event); }

  const egui::Input *getOutput();

  // Translate from window coordinates to local egui coordinates
  egui::Pos2 translatePointerPos(const egui::Pos2 &pos);

  // Applies egui output to update cursor pos, clipboard, etc.
  void applyOutput(const egui::IOOutput &output);

  // Set or clear the position for the text cursor
  void updateTextCursorPosition(const egui::Pos2 *pos);
  void copyText(const char *text);
  void updateCursorIcon(egui::CursorIcon icon);

  // Automatically called before translation to clear internal caches
  void reset();

  static EguiInputTranslator *create();
  static void destroy(EguiInputTranslator *renderer);
};
} // namespace gfx

namespace egui {
inline Pos2 toPos2(const gfx::float2 &v) { return Pos2{.x = v.x, .y = v.y}; }
inline Rect toRect(const gfx::float4 &v) {
  return Rect{
      .min = Pos2{.x = v.x, .y = v.y},
      .max = Pos2{.x = v.z, .y = v.w},
  };
}
} // namespace egui

#endif /* CA4EC896_C746_4AC6_A229_560CF01EEFEA */
