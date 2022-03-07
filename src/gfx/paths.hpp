#pragma once

#include <string>

namespace gfx {
const char *getDataRootPath();
void resolveDataPathInline(std::string &path);
std::string resolveDataPath(const std::string &path);
} // namespace gfx
