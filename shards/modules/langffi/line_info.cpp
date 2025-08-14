#include "line_info.hpp"
#include <shards/utility.hpp>
#include <boost/filesystem.hpp>
#include <unordered_map>
#include <shared_mutex>
#include <xxh3.h>
#include <spdlog/spdlog.h>

namespace shards::lang {
struct FileRegistry {
  static FileRegistry &fromHandle(SHFileRegistryHandle *handle) { return *reinterpret_cast<FileRegistry *>(handle); }

  std::unordered_map<std::string, uint32_t> pathToId;
  std::unordered_map<uint32_t, std::string> idToPath;
  std::shared_mutex mtx;

  uint32_t map(std::string_view path) {
    if (path.empty()) {
      return 0;
    }

    boost::filesystem::path p(path);
    auto npath = p.lexically_normal();
    auto gpath = npath.generic_string();
    if(gpath.empty()) {
      return 0;
    }

    // Normalize drive letter
    gpath[0] = std::toupper(gpath[0]);
    
    std::shared_lock lock(mtx);

    auto it = pathToId.find(gpath);
    if (it == pathToId.end()) {
      auto s = XXH3_createState();
      XXH3_64bits_reset(s);
      XXH3_64bits_update(s, gpath.data(), gpath.size());
      auto id = XXH3_64bits_digest(s);
      XXH3_freeState(s);

      lock.unlock();
      std::unique_lock lock2(mtx);

      pathToId[gpath] = id;
      idToPath[id] = gpath;
      SPDLOG_TRACE("line_info: Add {} => {}", gpath, id);
      return id;
    }

    return it->second;
  }

  std::string_view lookup(uint32_t file_id) {
    std::shared_lock lock(mtx);
    auto it = idToPath.find(file_id);
    if (it == idToPath.end()) {
      return std::string_view();
    }
    return it->second.c_str();
  }
};
extern "C" {
SHFileRegistryHandle *shlang_fr_static() {
  static shards::lang::FileRegistry instance;
  return (SHFileRegistryHandle *)&instance;
}
uint32_t shlang_fr_get_file_id(SHFileRegistryHandle *handle, SHStringWithLen path) {
  auto &fr = FileRegistry::fromHandle(handle);
  return fr.map(toStringView(path));
}
SHStringWithLen shlang_fr_get_file_name(SHFileRegistryHandle *handle, uint32_t file_id) {
  auto &fr = FileRegistry::fromHandle(handle);
  return toSWL(fr.lookup(file_id));
}
}
} // namespace shards::lang
