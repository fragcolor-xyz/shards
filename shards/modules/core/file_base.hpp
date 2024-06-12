#ifndef B700B4F5_BFEE_4FA6_B32C_46A88F2B0E35
#define B700B4F5_BFEE_4FA6_B32C_46A88F2B0E35

#include <shards/core/shared.hpp>
#include <boost/filesystem.hpp>
#include <shards/core/ops_internal.hpp>

namespace fs = boost::filesystem;

namespace shards {
inline bool getPathChecked(std::string &outFilename, const ParamVar &name_, bool checkExists = true) {
  const SHVar& name = name_.get();
  if (name.valueType != SHType::String) {
    SHLOG_DEBUG("FileBase: File parameter is not a string. ({}/{})", name, *name_);
    return false;
  }

  outFilename = SHSTRVIEW(name);

  // if absolute we are fine to begin with
  fs::path fp(outFilename);
  if (checkExists) {
    bool e = fs::exists(fp);
    if (!e) {
      SHLOG_DEBUG("FileBase: File does not exist. ({})", name);
    }
    return e;
  } else {
    return true;
  }
}
} // namespace shards

#endif /* B700B4F5_BFEE_4FA6_B32C_46A88F2B0E35 */
