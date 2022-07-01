#ifndef CA4EC896_C746_4AC6_A229_560CF01EEFEA
#define CA4EC896_C746_4AC6_A229_560CF01EEFEA

#include "egui_types.hpp"
#include <vector>

namespace gfx {
struct Window;
/// <div rustbindgen opaque></div>
struct EguiInputTranslator {
private:
  egui::Input input;
  std::vector<std::string> strings;
  std::vector<egui::InputEvent> events;
  egui::Pos2 lastCursorPosition;

public:
  const egui::Input* translateFromInputEvents(const std::vector<SDL_Event>& sdlEvents, Window& window, double time, float deltaTime);
};
} // namespace gfx

#endif /* CA4EC896_C746_4AC6_A229_560CF01EEFEA */
