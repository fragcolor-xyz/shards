#pragma once

#include "filesystem.hpp"
#include <string>

namespace gfx {
const char *getDataRootPath();
void resolveDataPathInline(fs::path &path);
fs::path resolveDataPath(const fs::path &path);
} // namespace gfx
