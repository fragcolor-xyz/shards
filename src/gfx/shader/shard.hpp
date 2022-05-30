#ifndef GFX_SHADER_SHARD
#define GFX_SHADER_SHARD

#include <memory>
#include <string>

namespace gfx {
namespace shader {
struct GeneratorContext;
namespace shards {
using String = std::string;
template <typename T> using UniquePtr = std::unique_ptr<T>;

struct Shard {
  virtual ~Shard() = default;
  virtual void apply(GeneratorContext &context) const = 0;
  virtual std::unique_ptr<Shard> clone() = 0;
};
using ShardPtr = UniquePtr<Shard>;

struct Direct : public Shard {
  String code;

  Direct(const char *code) : code(code) {}
  Direct(const String &code) : code(code) {}
  Direct(String &&code) : code(code) {}
  void apply(GeneratorContext &context) const;
  ShardPtr clone() { return std::make_unique<Direct>(code); }
};

struct ConvertibleToShard {
  UniquePtr<Shard> shard;
  ConvertibleToShard(const char *str) { shard = std::make_unique<Direct>(str); }
  ConvertibleToShard(UniquePtr<Shard> &&shard) : shard(std::move(shard)) {}
  UniquePtr<Shard> operator()() { return std::move(shard); }
};

template <typename T, typename T1 = void> struct ConvertToShard {};
template <typename T> struct ConvertToShard<T, typename std::enable_if<std::is_base_of_v<Shard, T>>::type> {
  UniquePtr<Shard> operator()(T &&shard) { return std::make_unique<T>(std::move(shard)); }
};
template <typename T> struct ConvertToShard<T, typename std::enable_if<!std::is_base_of_v<Shard, T>>::type> {
  UniquePtr<Shard> operator()(T &&arg) { return ConvertibleToShard(std::move(arg))(); }
};
} // namespace shards
} // namespace shader
} // namespace gfx

#endif // GFX_SHADER_SHARD
