#define CHAINBLOCKS_RUNTIME
#include "../core/runtime.hpp"

#include <iostream>
#include <fstream>
#include <set>

extern void NimMain();

int main(int argc, const char* argv[])
{
  // boot strap all
  NimMain();
  auto _ = chainblocks::createBlock("Const");

  std::ofstream os;
  os.open("CBGenerated.hpp");

  std::set<std::string> keywords;

  auto counter = 0;
  for(auto blockDef : chainblocks::BlocksRegister)
  {
    auto blockFullName = blockDef.first;
    LOG(INFO) << "Generating block: " << blockFullName;
    
    os << "BUILTIN_DEF(_Gen_" << counter << ", \"" << blockFullName << "\")\n";
    os << "{\n";
    os << "  auto block = chainblocks::createBlock(\"" << blockFullName << "\");\n";
    
    auto block = chainblocks::createBlock(blockDef.first.c_str());
    auto params = block->parameters(block);
    for(auto i = 0; i < stbds_arrlen(params); i++)
    {
      auto param = params[i];
      keywords.insert(param.name);
    }
    
    os << "  setBlockParameters(block, argsBegin, argsEnd);\n";
    os << "  return malValuePtr(new malCBBlock(block));\n";
    os << "}\n\n";
    
    counter++;
  }

  os << "void registerKeywords(malEnvPtr env)\n";
  os << "{\n";
  for(auto word : keywords)
  {
    os << "  env->set(\"" << word << "\", mal::keyword(\"" << word << "\"));\n";
  }
  os << "}\n\n";

  os.close();
  return 0;
}