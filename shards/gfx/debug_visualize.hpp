#ifndef F4134D2B_D14B_4E16_911C_7431B6636E6E
#define F4134D2B_D14B_4E16_911C_7431B6636E6E

namespace gfx {
struct ShapeRenderer;
struct IDebugVisualize {
  virtual void debugVisualize(ShapeRenderer &renderer) = 0;
};
} // namespace gfx

#endif /* F4134D2B_D14B_4E16_911C_7431B6636E6E */
