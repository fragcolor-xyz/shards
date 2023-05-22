#ifndef GFX_TEXTURE_FILE_TEXTURE_FILE
#define GFX_TEXTURE_FILE_TEXTURE_FILE

#include <gfx/fwd.hpp>

namespace gfx {
TexturePtr textureFromFile(const char *path);
TexturePtr textureFromFileFloat(const char *path);
} // namespace gfx

#endif // GFX_TEXTURE_FILE_TEXTURE_FILE
