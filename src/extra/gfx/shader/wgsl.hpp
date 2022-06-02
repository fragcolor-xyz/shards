#ifndef SH_EXTRA_GFX_SHADER_WGSL
#define SH_EXTRA_GFX_SHADER_WGSL

#include <gfx/shader/blocks.hpp>
#include <gfx/shader/types.hpp>
#include <gfx/shader/wgsl_mapping.hpp>
#include <nameof.hpp>
#include <stdexcept>
#include <stdint.h>

namespace gfx {
namespace shader {

struct IWGSLGenerated {
  virtual ~IWGSLGenerated() = default;

  virtual const FieldType &getType() const = 0;
  virtual blocks::BlockPtr toBlock() const = 0;
};

struct WGSLSource : public IWGSLGenerated {
  FieldType fieldType;
  std::string source;

  WGSLSource(FieldType fieldType, std::string &&source) : fieldType(fieldType), source(std::move(source)) {}

  const FieldType &getType() const { return fieldType; }
  blocks::BlockPtr toBlock() const { return blocks::makeBlock<blocks::Direct>(source); }
};

struct WGSLBlock : public IWGSLGenerated {
  FieldType fieldType;
  blocks::BlockPtr block;

  WGSLBlock(FieldType fieldType, blocks::BlockPtr &&block) : fieldType(fieldType), block(std::move(block)) {}

  const FieldType &getType() const { return fieldType; }
  blocks::BlockPtr toBlock() const { return block->clone(); }
};

} // namespace shader
} // namespace gfx

#endif // SH_EXTRA_GFX_SHADER_WGSL
