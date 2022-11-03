#include "core_shards.hpp"
#include "translate_wrapper.hpp"

namespace gfx::shader {
void registerCoreShards() {
  // Literal copy-paste into shader code
  REGISTER_SHADER_SHARD("Shader.Literal", Literal);
  REGISTER_SHADER_SHARD("Shader.ReadInput", gfx::shader::Read<blocks::ReadInput>);
  REGISTER_SHADER_SHARD("Shader.ReadBuffer", gfx::shader::ReadBuffer);
  REGISTER_SHADER_SHARD("Shader.ReadGlobal", gfx::shader::Read<blocks::ReadGlobal>);
  REGISTER_SHADER_SHARD("Shader.WriteGlobal", gfx::shader::Write<blocks::WriteGlobal>);
  REGISTER_SHADER_SHARD("Shader.WriteOutput", gfx::shader::Write<blocks::WriteOutput>);
  REGISTER_SHADER_SHARD("Shader.SampleTexture", gfx::shader::SampleTexture);
  REGISTER_SHADER_SHARD("Shader.SampleTextureUV", gfx::shader::SampleTextureUV);
  REGISTER_SHADER_SHARD("Shader.LinearizeDepth", gfx::shader::LinearizeDepth);

  // REGISTER_EXTERNAL_SHADER_SHARD(DoTranslator, "Do", shards::Const);
  REGISTER_EXTERNAL_SHADER_SHARD(ConstTranslator, "Const", shards::Const);
  REGISTER_EXTERNAL_SHADER_SHARD_T1(SetTranslator, "Set", shards::Set);
  REGISTER_EXTERNAL_SHADER_SHARD_T1(SetTranslator, "Ref", shards::Ref);
  REGISTER_EXTERNAL_SHADER_SHARD(GetTranslator, "Get", shards::Get);
  REGISTER_EXTERNAL_SHADER_SHARD(UpdateTranslator, "Update", shards::Update);
  REGISTER_EXTERNAL_SHADER_SHARD(TakeTranslator, "Take", shards::Take);
}
} // namespace gfx::shader