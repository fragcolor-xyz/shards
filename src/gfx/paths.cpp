#include "paths.hpp"
#include "filesystem.hpp"

#if _WIN32
#include <Windows.h>
#endif

namespace gfx {

#if !defined(GFX_DATA_PATH)
#if _WIN32
static inline std::string getModuleFilename() {
	std::string buffer;
	buffer.resize(GetModuleFileNameA(nullptr, nullptr, 0));
	buffer.resize(GetModuleFileNameA(nullptr, buffer.data(), buffer.size()));
	return buffer;
}
#endif

static fs::path findRootFolder(fs::path searchPath) {
	while (!searchPath.empty()) {
		fs::path testContentPath = searchPath;
		testContentPath /= "content";

		if (fs::exists(testContentPath))
			return searchPath;

		searchPath = searchPath.parent_path();
	}

	return fs::path();
}

static const char *findRootPath() {
	static std::string result;
	auto setResult = [&](fs::path strView) {
		result = strView.string();
		return result.c_str();
	};

	fs::path rootFolder;

#if _WIN32
	fs::path modulePath = getModuleFilename();

	rootFolder = findRootFolder(modulePath);
	if (!rootFolder.empty()) {
		return setResult(rootFolder);
	}
#endif

	fs::path currentFolder = fs::current_path();

	rootFolder = findRootFolder(currentFolder);
	if (!rootFolder.empty()) {
		return setResult(rootFolder);
	}

	// return current folder anyways
	return setResult(currentFolder);
}
#endif

const char *getDataRootPath() {
#if defined(GFX_DATA_PATH)
	const char *rootPath = GFX_DATA_PATH;
#else
	static const char *rootPath = findRootPath();
#endif
	return rootPath;
}

void resolveDataPathInline(fs::path &path) {
	fs::path inPath = path.c_str();
	if (inPath.is_absolute())
		return;

	path = getDataRootPath();
	path /= inPath;
}

fs::path resolveDataPath(const fs::path &path) {
	fs::path result = path;
	resolveDataPathInline(result);
	return result;
}
} // namespace gfx
