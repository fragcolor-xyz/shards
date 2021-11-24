#include "utils.hpp"
#include <SDL2/SDL_stdinc.h>

namespace gfx {

bool getEnvFlag(const char *name) {
	const char *value = SDL_getenv(name);
	if (!value)
		return false;

	int intValue = std::atoi(value);
	return intValue != 0;
}

} // namespace gfx
