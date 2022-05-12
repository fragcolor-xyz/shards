#ifndef CB_EXTRA_GFX_SHADER_TRANSLATOR
#define CB_EXTRA_GFX_SHADER_TRANSLATOR
#include "wgsl.hpp"
#include <gfx/shader/blocks.hpp>
#include <map>
#include <shards.hpp>
#include <spdlog/spdlog.h>
#include <string>

namespace gfx {
namespace shader {
template <typename T> using UniquePtr = std::unique_ptr<T>;

struct ShaderComposeError : public std::runtime_error {
  Shard *shard;

  ShaderComposeError(const char *what, Shard *shard = nullptr) : std::runtime_error(what), shard(shard){};
  ShaderComposeError(std::string &&what, Shard *shard = nullptr) : std::runtime_error(std::move(what)), shard(shard){};
};

struct IAppender {
  virtual ~IAppender() = default;
  virtual void append(blocks::Block *block, BlockPtr &&blockToAppend) = 0;
};

template <typename T> struct Appender : public IAppender {
  static inline IAppender *getInstance() { return nullptr; }
};

template <> struct Appender<blocks::Compound> : public IAppender {
  void append(blocks::Block *blockPtr, BlockPtr &&blockToAppend) {
    blocks::Compound *block = static_cast<blocks::Compound *>(blockPtr);
    block->children.emplace_back(std::move(blockToAppend));
  }

  static inline IAppender *getInstance() {
    static Appender instance;
    return &instance;
  }
};

// References a shader block together with a strategy for appending children into it
struct TranslationBlockRef {
  blocks::Block *block{};
  IAppender *appender{};

  TranslationBlockRef(blocks::Block * block, IAppender *appender) : block(block), appender(appender) {}

  template <typename T> static TranslationBlockRef make(blocks::Block *block) {
    return TranslationBlockRef(block, Appender<T>::getInstance());
  }

  template <typename T> static TranslationBlockRef make(const UniquePtr<T> &block) {
    return TranslationBlockRef(block.get(), Appender<T>::getInstance());
  }
};

struct TranslationRegistry;

// Context used during shader translations
// inputs C++ shards blocks
// outputs a shader block hierarchy defining a shader function
struct TranslationContext {
  TranslationRegistry &translationRegistry;

  spdlog::logger logger;

  // Generated dynamically
  std::map<std::string, FieldType> globals;

  UniquePtr<blocks::Compound> root;
  std::vector<TranslationBlockRef> stack;

  // The value wgsl source generated from the last shards block
  std::unique_ptr<IWGSLGenerated> wgslTop;

  TranslationContext();
  TranslationContext(const TranslationContext &) = delete;
  TranslationContext &operator=(const TranslationContext &) = delete;

  TranslationBlockRef &getTop() {
    assert(stack.size() >= 0);
    return stack.back();
  }

  // Add a new generated shader blocks without entering it
  template <typename T> void addNew(std::unique_ptr<T> &&ptr) {
    enterNew<T>(std::move(ptr));
    leave();
  }

  // Add a new generated shader block and push it on the stack
  template <typename T> void enterNew(std::unique_ptr<T> &&ptr) {
    TranslationBlockRef &top = getTop();
    if (!top.appender) {
      throw std::logic_error("Current shader block can not have children");
    }

    blocks::Block *newBlock = ptr.get();
    top.appender->append(getTop().block, std::move(ptr));
    stack.push_back(TranslationBlockRef::make<T>(newBlock));
  }

  // Remove a generated shader block from the stack
  void leave() { stack.pop_back(); }

  // Enter a shard and translate it recursively
  void processShard(ShardPtr shard);

  // Set the intermediate wgsl source generated from the last shard that was translated
  template <typename T, typename... TArgs> void setWGSLTop(TArgs... args) {
    wgslTop = std::make_unique<T>(std::forward<TArgs>(args)...);
  }

  void clearWGSLTop() { wgslTop.reset(); }

  std::unique_ptr<IWGSLGenerated> takeWGSLTop() {
    std::unique_ptr<IWGSLGenerated> result;
    wgslTop.swap(result);
    return result;
  }
};

// Handles translating shards C++ blocks to shader blocks
struct ITranslationHandler {
  virtual ~ITranslationHandler() = default;
  virtual void translate(ShardPtr shard, TranslationContext &context) = 0;
};

// Register that maps shard name to translation handlers
struct TranslationRegistry {
private:
  std::map<std::string, ITranslationHandler *> handlers;

public:
  void registerHandler(const char *shardName, ITranslationHandler *translateable);
  ITranslationHandler *resolve(Shard *shard);
};

} // namespace shader
} // namespace gfx

#endif // CB_EXTRA_GFX_SHADER_TRANSLATOR
