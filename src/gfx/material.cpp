#include "material.hpp"
#include "bgfx/bgfx.h"
#include "hasherxxh128.hpp"
#include "spdlog/spdlog.h"
#include <filesystem>
#include <fstream>
#include <memory>
#include <sstream>

namespace gfx {

void Material::modified() {
	HasherXXH128<HashStaticVistor> hasher;
	hasher(data);
	staticHash = hasher.getDigest();
}
} // namespace gfx