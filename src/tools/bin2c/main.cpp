#include <assert.h>
#include <bin2c/abi.hpp>
#include <fstream>
#include <spdlog/spdlog.h>
#include <sstream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vector>

// stb_compress* from stb.h - declaration
typedef unsigned int stb_uint;
typedef unsigned char stb_uchar;
stb_uint stb_compress(stb_uchar *out, stb_uchar *in, stb_uint len);

struct Options {
  const char *inPath;
  const char *outPath;
  const char *varName;
};

static int run_bin2c(const Options &options) {
  if (!options.inPath) {
    spdlog::error("No input file specified");
    return 1;
  }
  if (!options.outPath) {
    spdlog::error("No output file specified");
    return 1;
  }
  if (!options.varName) {
    spdlog::error("No variable name specified");
    return 1;
  }

  std::ifstream inFile;
  inFile.open(options.inPath, std::ios::binary);
  if (!inFile.is_open()) {
    spdlog::error("Failed to open input file {}", options.inPath);
  }

  using std::endl;

  std::ofstream outHeaderFile;
  outHeaderFile.open(options.outPath);

  std::ofstream outSourceFile;
  outSourceFile.open(std::string(options.outPath) + ".c");

  std::vector<uint8_t> data;
  inFile.seekg(0, std::ios::end);
  data.resize(inFile.tellg());
  inFile.seekg(0);
  inFile.read((char *)data.data(), data.size());

  uint8_t flags = bin2c::FLAGS_None;

  using WordType = uint64_t;
  const char *wordType = "uint64_t";
  size_t wordSize = 8;
  size_t numWords = (data.size() / wordSize + 1);

  size_t dataLength = data.size();
  size_t alignedLenth = numWords * wordSize;
  if (alignedLenth > data.size()) {
    data.resize(alignedLenth);
  }

  outSourceFile << "#include <stdint.h>" << endl;
  outSourceFile << fmt::format("static struct File {{ uint64_t length; uint64_t flags; {} data[{}]; }} _{} = ", wordType, numWords,
                               options.varName);
  outSourceFile << fmt::format("{{ {}, {}, {{", dataLength, flags);
  int column = 0;
  for (int i = 0; i < data.size(); i += wordSize) {
    WordType d = *(WordType *)(data.data() + i);
    if ((column++ % 12) == 0)
      outSourceFile << fmt::format("\n0x{:x},", d);
    else
      outSourceFile << fmt::format("0x{:x},", d);
  }
  outSourceFile << "} };" << endl;
  outSourceFile << fmt::format("void* {0} = &_{0};", options.varName) << endl;

  bool includeMinimalAPI = true;

  outHeaderFile << "#pragma once" << endl;
  if (includeMinimalAPI)
    outHeaderFile << "#include <stdint.h>" << endl;
  outHeaderFile << "#ifdef __cplusplus" << endl;
  outHeaderFile << "extern \"C\" {" << endl;
  outHeaderFile << "#endif" << endl;
  outHeaderFile << fmt::format("extern void* {};", options.varName) << endl;
  outHeaderFile << "#ifdef __cplusplus" << endl;
  outHeaderFile << "}" << endl;
  outHeaderFile << "#endif" << endl;

  if (includeMinimalAPI) {
    outHeaderFile << fmt::format("inline size_t {0}_getLength()"
                                 "{{ size_t* sizePtr = (size_t*)((uint8_t*){0} + {1}); return *sizePtr; }}",
                                 options.varName, offsetof(bin2c::File, length))
                  << endl;
    outHeaderFile << fmt::format("inline const uint8_t* {0}_getData()"
                                 "{{ const uint8_t* dataPtr = ((uint8_t*){0} + {1}); return dataPtr; }}",
                                 options.varName, offsetof(bin2c::File, data))
                  << endl;
  }

  return 0;
}

int main(int argc, char **argv) {
  if (argc < 3) {
    spdlog::warn("Syntax: {} -in <inputfile> -out <outputfile> -varName <symbolname>", argv[0]);
    return 0;
  }

  Options options{};

  int argIndex = 1;
  while (argIndex < argc) {
    bool argError = false;
    auto checkArg = [&](const char *name) {
      if (strcmp(argv[argIndex], name) == 0) {
        argIndex++;
        if (argIndex >= argc) {
          spdlog::error("option {} requires another argument", name);
          argError = true;
          return false;
        }
        return true;
      } else {
        return false;
      }
    };

    if (checkArg("-in")) {
      options.inPath = argv[argIndex];
      argIndex++;
    } else if (checkArg("-out")) {
      options.outPath = argv[argIndex];
      argIndex++;
    } else if (checkArg("-varName")) {
      options.varName = argv[argIndex];
      argIndex++;
    } else if (argError) {
      return 1;
    } else {
      spdlog::error("Unknown argument: '{}'", argv[argIndex]);
      return 1;
    }
  }

  return run_bin2c(options);
}
