#include "paths.hpp"
#include "bx/string.h"
#include <bx/file.h>
#include <bx/filepath.h>

#if _WIN32
#include <Windows.h>
#endif

namespace gfx {

#if _WIN32
static inline std::string getModuleFilename() {
  char path[bx::kMaxFilePath];
  GetModuleFileNameA(nullptr, path, sizeof(path));
  return path;
}
#endif

static bx::StringView getParentDirPath(const bx::FilePath &path) {
  bx::StringView inPathString = path;
  if (inPathString.isEmpty())
    return inPathString;

  bx::FilePath fpNonTerm =
      bx::StringView(inPathString.getPtr(), inPathString.getTerm() - 1);
  return fpNonTerm.getPath();
}

static bx::FilePath findRootFolder(bx::FilePath searchPath) {
  while (!searchPath.isEmpty()) {
    bx::FilePath testContentPath = searchPath;
    testContentPath.join("content");

    bx::FileInfo fileInfo;
    if (bx::stat(fileInfo, testContentPath))
      return searchPath;

    searchPath = getParentDirPath(searchPath);
  }

  return bx::FilePath();
}

static const char *findRootPath() {
  static std::string result;
  auto setResult = [&](bx::StringView strView) {
    result = std::string(strView.getPtr(), strView.getTerm());
    return result.c_str();
  };

  bx::FilePath rootFolder;

#if _WIN32
  bx::FilePath moduleFilename = getModuleFilename().c_str();
  bx::StringView modulePath = moduleFilename.getPath();

  rootFolder = findRootFolder(modulePath);
  if (!rootFolder.isEmpty()) {
    return setResult(rootFolder);
  }
#endif

  bx::FilePath currentFolder = bx::FilePath(bx::Dir::Current).getCPtr();

  rootFolder = findRootFolder(currentFolder);
  if (!rootFolder.isEmpty()) {
    return setResult(rootFolder);
  }

  // return current folder anyways
  return setResult(currentFolder);
}

const char *getDataRootPath() {
#if defined(GFX_DATA_PATH)
  const char *rootPath = GFX_DATA_PATH;
#else
  static const char *rootPath = findRootPath();
#endif
  return rootPath;
}

void resolveDataPathInline(std::string &path) {
  bx::FilePath inPath = path.c_str();
  if (inPath.isAbsolute())
    return;

  bx::FilePath bxPath = getDataRootPath();
  bxPath.join(inPath);
  path = bxPath.getCPtr();
}

std::string resolveDataPath(const std::string &path) {
  std::string result = path;
  resolveDataPathInline(result);
  return result;
}
} // namespace gfx
