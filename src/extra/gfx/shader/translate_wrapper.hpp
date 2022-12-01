#ifndef DC395A77_FF75_4BA7_8A6C_98150E2BE3CB
#define DC395A77_FF75_4BA7_8A6C_98150E2BE3CB

#include "shardwrapper.hpp"
#include "translator.hpp"

namespace gfx::shader {

SH_HAS_MEMBER_TEST(translate);

template <typename TShard> struct TranslateWrapper : public ITranslationHandler {
  using ShardWrapper = shards::ShardWrapper<TShard>;

  void translate(ShardPtr shard, TranslationContext &context) {
    ShardWrapper *wrapper = reinterpret_cast<ShardWrapper *>(shard);
    TShard *innerShard = &wrapper->shard;

    static_assert(has_translate<TShard>::value, "Shader shards must have a \"translate\" method.");
    if constexpr (has_translate<TShard>::value) {
      innerShard->translate(context);
    }
  }
};

template <typename TShard> struct TranslateWrapperExternal : public ITranslationHandler {
  using ShardWrapper = shards::ShardWrapper<TShard>;
  using Callback = std::function<void(TShard *shard, TranslationContext &)>;

  Callback callback;

  TranslateWrapperExternal(Callback callback) : callback(callback){};
  void translate(ShardPtr shard, TranslationContext &context) {
    ShardWrapper *wrapper = reinterpret_cast<ShardWrapper *>(shard);
    TShard *innerShard = &wrapper->shard;

    callback(innerShard, context);
  }
};

// Translator is called as instance shard.translate(context&)
#define REGISTER_SHADER_SHARD(_name, _class)                    \
  {                                                             \
    static TranslateWrapper<_class> instance{};                 \
    getTranslationRegistry().registerHandler(_name, &instance); \
    REGISTER_SHARD(_name, _class);                              \
  }

// Translator is called as static TranslatorClass::translate(ShardClass*, context&)
#define REGISTER_EXTERNAL_SHADER_SHARD(_translatorClass, _name, _class)             \
  {                                                                                 \
    static TranslateWrapperExternal<_class> instance(&_translatorClass::translate); \
    getTranslationRegistry().registerHandler(_name, &instance);                     \
  }

// Translator is called as static TranslatorClass<TShard>::translate(TShard*, context&)
#define REGISTER_EXTERNAL_SHADER_SHARD_T1(_translatorClass, _name, _class)                  \
  {                                                                                         \
    static TranslateWrapperExternal<_class> instance(&_translatorClass<_class>::translate); \
    getTranslationRegistry().registerHandler(_name, &instance);                             \
  }

// Translator is called as static TranslatorClass<TShard, T1>::translate(TShard*, context&)
#define REGISTER_EXTERNAL_SHADER_SHARD_T2(_translatorClass, _name, _class, _t1)                  \
  {                                                                                              \
    static TranslateWrapperExternal<_class> instance(&_translatorClass<_class, _t1>::translate); \
    getTranslationRegistry().registerHandler(_name, &instance);                                  \
  }

} // namespace gfx::shader

#endif /* DC395A77_FF75_4BA7_8A6C_98150E2BE3CB */
