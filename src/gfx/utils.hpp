#pragma once

#include <vector>
#include <cinttypes>
#include <string>

namespace gfx {

bool getEnvFlag(const char *name);
std::string extractPlatformShaderBinary(const std::vector<uint8_t>& inData);

} // namespace gfx
