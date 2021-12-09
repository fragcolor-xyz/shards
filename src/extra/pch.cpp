#if _WIN32
#include <winsock2.h>
#endif

#include <chrono>
#include <iostream>
#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <deque>
#include <iomanip>
#include <functional>
#include <memory>
#include <list>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <utility>
#include <sstream>
// #include <type_traits>
#include <variant>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <cstring>

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
