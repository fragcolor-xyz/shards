#pragma once

#include <cinttypes>
#include <string>
#include <vector>

namespace gfx {

bool getEnvFlag(const char *name);
std::string extractPlatformShaderBinary(const std::vector<uint8_t> &inData);

} // namespace gfx
