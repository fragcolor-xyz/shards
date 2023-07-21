#ifndef B700B4F5_BFEE_4FA6_B32C_46A88F2B0E35
#define B700B4F5_BFEE_4FA6_B32C_46A88F2B0E35

#include <shards/core/shared.hpp>
#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;

namespace shards {
struct FileBase {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static inline Parameters params{{"File", SHCCSTR("The file to read/write from."), {CoreInfo::StringStringVarOrNone}}};

  ParamVar _filename{};
  OwnedVar _currentFileName{};

  void cleanup() { _filename.cleanup(); }
  void warmup(SHContext *context) { _filename.warmup(context); }

  static SHParametersInfo parameters() { return params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _filename = value;
      cleanup();
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    auto res = SHVar();
    switch (index) {
    case 0:
      return _filename;
    default:
      break;
    }
    return res;
  }

  bool getFilename(SHContext *context, std::string &filename, bool checkExists = true) {
    auto &ctxFile = _filename.get();
    if (ctxFile.valueType != SHType::String)
      return false;

    filename = SHSTRVIEW(ctxFile);
    _currentFileName = _filename.get();

    // if absolute we are fine to begin with
    fs::path fp(filename);
    if (checkExists) {
      return fs::exists(fp);
    } else {
      return true;
    }
  }
};
} // namespace shards

#endif /* B700B4F5_BFEE_4FA6_B32C_46A88F2B0E35 */
