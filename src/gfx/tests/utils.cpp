#include "utils.hpp"
#include <bx/file.h>
#include <cassert>
#include <string>
#include <vector>

#ifdef __EMSCRIPTEN__
#include <emscripten/emscripten.h>

void emrun_copyFileToOutput(const char *filePath) {
	bx::Error err;
	bx::FileReader reader;
	assert(bx::open(&reader, filePath));
	std::vector<uint8_t> imageData;
	imageData.resize(reader.seek(0, bx::Whence::End));
	reader.seek(0, bx::Whence::Begin);
	reader.read(imageData.data(), imageData.size(), &err);
	reader.close();

	std::string filePathStr = filePath;
	if (filePathStr.find('/') == 0)
		filePathStr = filePathStr.substr(1);

	EM_ASM_ARGS(
		{
			var filePath = UTF8ToString($0);
			console.log("Copying file to dump_out", filePath, $2);
			emrun_file_dump(filePath, HEAPU8.subarray($1, $1 + $2));
		},
		filePathStr.c_str(), imageData.data(), imageData.size());
}
#endif