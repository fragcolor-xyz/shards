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

struct Input {
  int cursorPosition[2];
  bool mouseButton{};
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
