#ifndef CA4EC896_C746_4AC6_A229_560CF01EEFEA
#define CA4EC896_C746_4AC6_A229_560CF01EEFEA

#include "egui_types.hpp"
#include <vector>

namespace gfx {
struct Window;
struct EguiInputTranslatorPrivate;

/// <div rustbindgen opaque></div>
struct EguiInputTranslator {
private:
  egui::Input input;
  std::vector<std::string> strings;
  std::vector<egui::InputEvent> events;
  egui::Pos2 lastCursorPosition;
  bool imeComposing{};
  bool textInputActive{};

public:
  EguiInputTranslator() = default;
  EguiInputTranslator(const EguiInputTranslator &) = delete;
  const EguiInputTranslator &operator=(const EguiInputTranslator &) = delete;

  const egui::Input *translateFromInputEvents(const std::vector<SDL_Event> &sdlEvents, Window &window, double time,
                                              float deltaTime);

  // Set or clear the position for the text cursor
  void updateTextCursorPosition(Window &window, const egui::Pos2 *pos);
  void copyText(const char *text);
  void updateCursorIcon(egui::CursorIcon icon);

  // Automatically called before translation to clear internal caches
  void reset();

  static EguiInputTranslator *create();
  static void destroy(EguiInputTranslator *renderer);
};
} // namespace gfx

#endif /* CA4EC896_C746_4AC6_A229_560CF01EEFEA */
