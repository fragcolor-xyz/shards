#include "api.hpp"
#include "bx/error.h"
#include "bx/file.h"
#include "bx/filepath.h"
#include "bx/string.h"
#include <atomic>
#include <bx/file.h>
#include <bx/readerwriter.h>
#include <stdint.h>

#undef BX_TRACE
#undef BX_WARN
#undef BX_ASSERT
#include "shaderc.h"

#ifdef gfx_shaderc_EXPORTS
#if defined(_WIN32)
#define SHADERC_API __declspec(dllexport)
#else
#define SHADERC_API __attribute__((visibility("default")))
#endif
#else
#define SHADERC_API
#endif

namespace bgfx {
bool compileShader(const char *_varying, const char *_comment, char *_shader, uint32_t _shaderLen, bgfx::Options &_options, bx::WriterI *_writer);
extern bool g_verbose;
} // namespace bgfx

struct ShaderCompiler : public gfx::IShaderCompiler {
	virtual bool compile(gfx::IShaderCompileOptions &options, gfx::IShaderCompileOutput &output, const char *varying, const char *code, size_t codeLength) {

		const gfx::ShaderCompilePODOptions &podOptions = options.getPODOptions();

		bgfx::Options bgfxOptions;
		bgfxOptions.shaderType = podOptions.shaderType;
		bgfxOptions.disasm = podOptions.disasm;
		bgfxOptions.raw = podOptions.raw;
		bgfxOptions.preprocessOnly = podOptions.preprocessOnly;
		bgfxOptions.depends = podOptions.depends;
		bgfxOptions.debugInformation = podOptions.debugInformation;
		bgfxOptions.avoidFlowControl = podOptions.avoidFlowControl;
		bgfxOptions.noPreshader = podOptions.noPreshader;
		bgfxOptions.partialPrecision = podOptions.partialPrecision;
		bgfxOptions.preferFlowControl = podOptions.preferFlowControl;
		bgfxOptions.backwardsCompatibility = podOptions.backwardsCompatibility;
		bgfxOptions.warningsAreErrors = podOptions.warningsAreErrors;
		bgfxOptions.keepIntermediate = podOptions.keepIntermediate;
		bgfxOptions.optimize = podOptions.optimize;
		bgfxOptions.optimizationLevel = podOptions.optimizationLevel;

		bgfxOptions.platform = options.getPlatform();
		bgfxOptions.profile = options.getProfile();
		bgfxOptions.inputFilePath = options.getInputFilePath();
		bgfxOptions.outputFilePath = options.getOutputFilePath();

#define CONVERT_ARRAY(_res, _get, _size)   \
	for (size_t i = 0; i < (_size); i++) { \
		(_res).push_back(_get(i));         \
	}

		CONVERT_ARRAY(bgfxOptions.includeDirs, options.getIncludeDir, options.getNumIncludeDirs())
		CONVERT_ARRAY(bgfxOptions.defines, options.getDefine, options.getNumDefines())
		CONVERT_ARRAY(bgfxOptions.dependencies, options.getDependency, options.getNumDependencies())

		struct WriterWrapper : public bx::WriterI {
			gfx::IShaderCompileOutput &output;
			WriterWrapper(gfx::IShaderCompileOutput &output) : output(output) {}
			virtual int32_t write(const void *_data, int32_t _size, bx::Error *_err) {
				output.writeOutput(_data, _size);
				return _size;
			}
		};
		WriterWrapper writer(output);

		// bgfx::compileShader calls delete[] on this
		char *codeCopy = new char[codeLength + 1];
		memcpy(codeCopy, code, codeLength + 1);

		std::string tempFilename = []() {
			static std::atomic<uint64_t> atomicCounter = 0;

			std::string result;
			uint64_t counter = atomicCounter.fetch_add(1);
			bx::stringPrintf(result, "_shaderc%04llu", counter);

			return result;
		}();

		std::string tempSourcePath = tempFilename + ".sh";

		bx::FileWriter tempFileWriter;
		if (!bx::open(&tempFileWriter, tempSourcePath.c_str())) {
			return false;
		}

		// Need to store shader code in a file for the preprocessor library
		bx::Error err;
		tempFileWriter.write(codeCopy, codeLength, &err);
		tempFileWriter.close();

		bgfxOptions.inputFilePath = tempSourcePath;
		bgfxOptions.outputFilePath = tempFilename;

		bgfx::g_verbose = podOptions.verbose;
		bool shaderCompiled = bgfx::compileShader(varying, "", codeCopy, codeLength, bgfxOptions, &writer);

		// remove temp files
		if (!podOptions.keepOutputs) {
			bx::remove(tempSourcePath.c_str());
			bx::remove((tempFilename + ".hlsl").c_str());
			bx::remove((tempFilename + ".d").c_str());
		}

		return shaderCompiled;
	}
};

extern "C" {
gfx::IShaderCompiler *SHADERC_API GFX_SHADERC_MODULE_ENTRY() { return new ShaderCompiler(); }
}