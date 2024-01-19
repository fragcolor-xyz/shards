#ifndef SH_EXTRA_GFX_SHADER_WGSL
#define SH_EXTRA_GFX_SHADER_WGSL

#include <gfx/shader/blocks.hpp>
#include <gfx/shader/types.hpp>
#include <gfx/shader/wgsl_mapping.hpp>
#include <nameof.hpp>
#include <stdexcept>
#include <stdint.h>
#include <vector>

namespace gfx {
namespace shader {

struct IWGSLGenerated {
  virtual ~IWGSLGenerated() = default;

  virtual const Type &getType() const = 0;
  virtual blocks::BlockPtr toBlock() const = 0;
};

struct WGSLSource : public IWGSLGenerated {
  Type fieldType;
  std::string source;

  WGSLSource(Type fieldType, std::string &&source) : fieldType(fieldType), source(std::move(source)) {}

  const Type &getType() const { return fieldType; }
  blocks::BlockPtr toBlock() const { return blocks::makeBlock<blocks::Direct>(source); }
};

struct WGSLBlock : public IWGSLGenerated {
  Type fieldType;
  blocks::BlockPtr block;

  WGSLBlock(WGSLBlock &&) = default;
  WGSLBlock(Type fieldType, blocks::BlockPtr &&block) : fieldType(fieldType), block(std::move(block)) {}

  const Type &getType() const { return fieldType; }
  blocks::BlockPtr toBlock() const { return block->clone(); }
};

} // namespace shader
} // namespace gfx

#endif // SH_EXTRA_GFX_SHADER_WGSL
