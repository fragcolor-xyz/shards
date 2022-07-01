#ifndef DBE976AE_DBE6_4455_8698_E341D3B8223F
#define DBE976AE_DBE6_4455_8698_E341D3B8223F

#include <stdint.h>
#include <stddef.h>

#ifndef RUST_BINDGEN
#include <vector>
#include "../mesh.hpp"
#endif

namespace egui {
struct Pos2 {
  float x, y;
};

struct Rect {
  Pos2 min, max;
};

struct Color32 {
  uint8_t r, g, b, a;
};

struct Vertex {
  Pos2 pos;
  Pos2 uv;
  Color32 color;

#ifndef RUST_BINDGEN
  static std::vector<gfx::MeshVertexAttribute> getAttributes();
#endif
};

struct TouchDeviceId {
  uint64_t id;
  operator uint64_t() const { return id; }
};

struct TouchId {
  uint64_t id;
  operator uint64_t() const { return id; }
};

enum class InputEventType : uint8_t {
  Copy,
  Cut,
  Paste,
  Text,
  Key,
  PointerMoved,
  PointerButton,
  PointerGone,
  Scroll,
  Zoom,
  CompositionStart,
  CompositionUpdate,
  CompositionEnd,
  Touch
};

enum class TouchPhase : uint8_t {
  Start,
  Move,
  End,
  Cancel,
};

enum class PointerButton : uint8_t {
  Primary = 0,
  Secondary = 1,
  Middle = 2,
};

struct ModifierKeys {
  bool alt;
  bool ctrl;
  bool shift;
  bool macCmd;
  bool command;
};

union InputEvent {
  struct {
    InputEventType type;
  } common;
  struct {
    InputEventType type;
  } copy;
  struct {
    InputEventType type;
  } cut;
  struct {
    InputEventType type;
    const char *str;
  } paste;
  struct {
    InputEventType type;
    const char *str;
  } text;
  struct {
    InputEventType type;
    Pos2 pos;
  } pointerMoved;
  struct {
    InputEventType type;
    Pos2 pos;
    PointerButton button;
    bool pressed;
    ModifierKeys modifiers;
  } pointerButton;
  struct {
    InputEventType type;
  } pointerGone;
  struct {
    InputEventType type;
    Pos2 delta;
  } scroll;
  struct {
    InputEventType type;
    float delta;
  } zoom;
  struct {
    InputEventType type;
  } compositionStart;
  struct {
    InputEventType type;
    const char *text;
  } compositionUpdate;
  struct {
    InputEventType type;
    const char *text;
  } compositionEnd;
  struct {
    InputEventType type;
    TouchDeviceId deviceId;
    TouchId id;
    TouchPhase phase;
    Pos2 pos;
    float force;
  } touch;
};

struct HoveredFile {
  const char *path;
  const char *mime;
};

struct DroppedFile {
  const char *path;
  const char *name;
  const char *lastModified; // ISO 8601 timestamp / strftime("%FT%TZ")
  const uint8_t *data;
  size_t dataLength;
};

struct Input {
  Rect screenRect;
  float pixelsPerPoint;
  double time;
  float predictedDeltaTime;
  ModifierKeys modifierKeys;

  const InputEvent *inputEvents;
  size_t numInputEvents;

  const HoveredFile *hoveredFiles;
  size_t numHoveredFiles;

  const DroppedFile *droppedFiles;
  size_t numDroppedFiles;
};

struct TextureId {
  uint64_t id;
  operator uint64_t() const { return id; }
};

enum class TextureFormat : uint8_t { R32F, RGBA8 };

struct TextureSet {
  TextureId id;
  size_t size[2];
  size_t offset[2];
  bool subRegion = false;
  const uint8_t *pixels;
  TextureFormat format;
};

struct TextureUpdates {
  const TextureSet *sets;
  size_t numSets;
  const TextureId *frees;
  size_t numFrees;
};

struct ClippedPrimitive {
  Rect clipRect;
  const uint32_t *indices;
  size_t numIndices;
  const Vertex *vertices;
  size_t numVertices;
  TextureId textureId;
};

struct FullOutput {
  TextureUpdates textureUpdates;
  const ClippedPrimitive *primitives;
  size_t numPrimitives;
};
} // namespace egui

#endif /* DBE976AE_DBE6_4455_8698_E341D3B8223F */
