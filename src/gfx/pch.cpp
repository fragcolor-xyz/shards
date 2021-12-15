#include <spdlog/common.h>
#include <spdlog/spdlog.h>

#include <SDL_events.h>
#include <SDL_video.h>

#include <memory>
#include <stdint.h>
#include <vector>
#include <deque>
#include <stdexcept>
#include <unordered_map>
#include <cassert>

#include <bgfx/bgfx.h>
#include <bgfx/defines.h>
#include <bgfx/platform.h>
#include <bx/os.h>

#include <linalg/linalg.h>
