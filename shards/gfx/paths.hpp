#ifndef GFX_PATHS
#define GFX_PATHS

#include "filesystem.hpp"
#include <string>

namespace gfx {
const char *getDataRootPath();
void resolveDataPathInline(fs::path &path);
fs::path resolveDataPath(const fs::path &path);
} // namespace gfx

#endif // GFX_PATHS
