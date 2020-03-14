#ifndef CB_LMDB_HPP
#define CB_LMDB_HPP

#include "shared.hpp"
#include <filesystem>
#include <lmdb.h>

namespace chainblocks {
namespace LMDB {
struct Env {
  bool init = false;
  MDB_env *env;

#define CHECKED(__call__)                                                      \
  {                                                                            \
    auto err = __call__;                                                       \
    if (err != 0) {                                                            \
      LOG(ERROR) << "LMDB error: " << mdb_strerror(err);                       \
      throw CBException("LMDB error detected.");                               \
    }                                                                          \
  }

  Env() {
    CHECKED(mdb_env_create(&env));

    unsigned int flags = MDB_NOTLS | MDB_NOSYNC | MDB_NOSUBDIR;

    std::filesystem::path dbPath("cbdb.bin");
    if (Globals::RootPath.size() > 0) {
      std::filesystem::path cbpath(Globals::RootPath);
      auto abspath = std::filesystem::absolute(cbpath / dbPath);
      abspath.make_preferred();
      CHECKED(mdb_env_open(env, abspath.string().c_str(), flags, 664));
    } else {
      auto abspath =
          std::filesystem::absolute(std::filesystem::current_path() / dbPath);
      abspath.make_preferred();
      CHECKED(mdb_env_open(env, abspath.string().c_str(), flags, 664));
    }

    // we sync every global "sleep" only!
    registerRunLoopCallback("cb.lmdb.sync", []() {
      static const Env &env{Singleton<Env>::value};
      CHECKED(mdb_env_sync(env.env, 1));
    });
  }
};

struct Get {
  const Env &env{Singleton<Env>::value};
};

struct Set {
  const Env &env{Singleton<Env>::value};
};

struct Iterate {
  const Env &env{Singleton<Env>::value};
};

struct Delete {
  const Env &env{Singleton<Env>::value};
};
} // namespace LMDB
} // namespace chainblocks

#endif /* CB_LMDB_HPP */
