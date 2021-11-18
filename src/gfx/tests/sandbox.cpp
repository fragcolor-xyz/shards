#include "bx/file.h"
#include "bx/filepath.h"
#include "bx/error.h"
#include "bx/readerwriter.h"
#include "gfx/shader_paths.hpp"
#include "spdlog/spdlog.h"
#include <bx/file.h>
#include <bx/filepath.h>
#include <spdlog/spdlog.h>
#include <string>

using namespace gfx;

int main(int argc, char **argv) {
	bx::FileReader fr;
	if (bx::open(&fr, "deps/bgfx/src/bgfx_shader.sh")) {
		bx::Error err;
		char buff[4096];
		int32_t l = fr.read(buff, sizeof(buff), &err);
		std::string val(buff, buff + l);
		spdlog::info("Value: {}", val);
	}
	
	spdlog::info("Hello world");
	for (auto &includePath : gfx::shaderIncludePaths) {
		bx::FilePath searchPath = "/";
		// searchPath.join(includePath);
		
		spdlog::info("Shader include path: {}", searchPath.getCPtr());
		bx::DirectoryReader dir;
		bx::Error err;
		if (!bx::open(&dir, searchPath, &err)) {
			std::string errMsg(err.getMessage().getPtr(), err.getMessage().getTerm());
			spdlog::error("Failed to open folder: {}", errMsg.c_str());
			continue;
		}

		bx::FileInfo fi;
		while (true) {
			bx::read(&dir, fi, &err);
			if (!err.isOk())
				break;
			spdlog::info(" -:{}", fi.filePath.getCPtr());
		}

		dir.close();
	}
}