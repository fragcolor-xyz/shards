#include "utils.hpp"
#include <SDL_stdinc.h>
#include <bx/hash.h>
#include <bx/readerwriter.h>
#include <bx/string.h>

namespace gfx {

bool getEnvFlag(const char *name) {
  const char *value = SDL_getenv(name);
  if (!value)
    return false;

  int intValue = std::atoi(value);
  return intValue != 0;
}

std::string extractPlatformShaderBinary(const std::vector<uint8_t> &data) {
  const uint8_t *dataPtr = data.data();

  bx::MemoryReader reader(dataPtr, data.size());
  (void)bx::hash<bx::HashMurmur2A>(dataPtr, data.size());

  uint32_t magic;
  bx::read(&reader, magic);

  uint32_t hashIn;
  bx::read(&reader, hashIn);

  uint32_t hashOut;
  bx::read(&reader, hashOut);

  uint16_t count;
  bx::read(&reader, count);

  for (uint32_t ii = 0; ii < count; ++ii) {
    uint8_t nameSize = 0;
    bx::read(&reader, nameSize);

    char name[256];
    bx::read(&reader, &name, nameSize);
    name[nameSize] = '\0';

    uint8_t type;
    bx::read(&reader, type);

    uint8_t num;
    bx::read(&reader, num);

    uint16_t regIndex;
    bx::read(&reader, regIndex);

    uint16_t regCount;
    bx::read(&reader, regCount);

    uint16_t texInfo = 0;
    bx::read(&reader, texInfo);

    uint16_t texFormat = 0;
    bx::read(&reader, texFormat);
  }

  uint32_t shaderSize;
  bx::read(&reader, shaderSize);

  const uint8_t *sourceDataPtr = reader.getDataPtr();

  return std::string(sourceDataPtr, sourceDataPtr + shaderSize);
}

} // namespace gfx
