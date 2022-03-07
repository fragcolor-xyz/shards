#if _WIN32
#include <winsock2.h>
#endif

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <deque>
#include <functional>
#include <iomanip>
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <utility>
// #include <type_traits>
#include <cstring>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <variant>
#include <vector>

#include <string.h>

#include <boost/align/aligned_allocator.hpp>
#include <boost/asio/dispatch.hpp>
#include <boost/asio/thread_pool.hpp>

#ifndef __EMSCRIPTEN__
#include <boost/context/continuation.hpp>
#endif

#if _WIN32
#include <Windows.h>
#endif

#define XXH_INLINE_ALL
#include <xxhash.h>

#include <tinygltf/tiny_gltf.h>
