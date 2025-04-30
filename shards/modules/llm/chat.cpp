#include "shared.hpp"

#include "../../../deps/llama.cpp/examples/llava/clip-impl.h"
#include "../../../deps/llama.cpp/common/sampling.h"

#include <string>
#include <vector>
#include <mutex>

int LLAMA_BUILD_NUMBER = 1;
char const *LLAMA_COMMIT = "1";
char const *LLAMA_COMPILER = "shards";
char const *LLAMA_BUILD_TARGET = "unknown";

namespace shards {
namespace llm {
// Data structure for MultiModal Chat
struct ChatData {
  // LLM components
  OwnedVar _modelData; // Reference to the LLM model
  std::shared_ptr<llama_context> ctx;
  std::shared_ptr<std::mutex> _mutex;
  common_params params{};
  llama_batch batch;

  ChatData() { batch = llama_batch_init(params.n_batch, 0, 1); }

  // CLIP model components
  struct clip_ctx *clip_ctx = nullptr;
  std::string mmproj_path;
  int n_threads = 1;

  // State tracking
  llama_pos n_past = 0;
  bool is_generating = false;

  ~ChatData() {
    if (clip_ctx) {
      clip_free(clip_ctx);
      clip_ctx = nullptr;
    }
    llama_batch_free(batch);
  }
};

// Helper for handling embedded images
struct decode_embd_batch {
  std::vector<llama_pos> pos;
  std::vector<int32_t> n_seq_id;
  std::vector<llama_seq_id> seq_id_0;
  std::vector<llama_seq_id *> seq_ids;
  std::vector<int8_t> logits;
  llama_batch batch;

  decode_embd_batch(float *embd, int32_t n_tokens, llama_pos pos_0, llama_seq_id seq_id) {
    pos.resize(n_tokens);
    n_seq_id.resize(n_tokens);
    seq_ids.resize(n_tokens + 1);
    logits.resize(n_tokens);
    seq_id_0.resize(1);
    seq_id_0[0] = seq_id;
    seq_ids[n_tokens] = nullptr;
    batch = {
        /*n_tokens      =*/n_tokens,
        /*tokens        =*/nullptr,
        /*embd          =*/embd,
        /*pos           =*/pos.data(),
        /*n_seq_id      =*/n_seq_id.data(),
        /*seq_id        =*/seq_ids.data(),
        /*logits        =*/logits.data(),
    };
    for (int i = 0; i < n_tokens; i++) {
      batch.pos[i] = pos_0 + i;
      batch.n_seq_id[i] = 1;
      batch.seq_id[i] = seq_id_0.data();
      batch.logits[i] = false;
    }
  }
};

// The MultiModal Chat shard
struct Chat {
  static inline int32_t ObjectId = 'llch';
  static inline const char VariableName[] = "LLM.Chat";
  static inline ::shards::Type Type = ::shards::Type::Object(CoreCC, ObjectId);
  static inline SHTypeInfo RawType = Type;
  static inline ::shards::Type VarType = ::shards::Type::VariableOf(Type);
  static inline shards::ObjectVar<ChatData> ObjectVar{VariableName, RawType.object.vendorId, RawType.object.typeId};

  Chat() {
    _contextSize = Var(1024);
    _threads = Var(4);
  }

  static SHTypesInfo inputTypes() { return ModelData::Type; } // Takes LLM.Model
  static SHTypesInfo outputTypes() { return Type; }

  PARAM_PARAMVAR(_contextSize, "ContextSize", "The size of the context window",
                 {shards::CoreInfo::IntType, shards::CoreInfo::IntVarType});
  PARAM_PARAMVAR(_mmproj, "MMProj", "Path to the multimodal projector model",
                 {shards::CoreInfo::NoneType, shards::CoreInfo::StringType, shards::CoreInfo::StringVarType});
  PARAM_PARAMVAR(_threads, "Threads", "Number of threads to use for image processing",
                 {shards::CoreInfo::IntType, shards::CoreInfo::IntVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_contextSize), PARAM_IMPL_FOR(_mmproj), PARAM_IMPL_FOR(_threads));

  ChatData *_data{};

  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);
    if (_data) {
      ObjectVar.Release(_data);
      _data = nullptr;
    }
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _data = ObjectVar.New();
  }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    // Extract the LLM model from input
    auto &modelData = varAsObjectChecked<ModelData>(input, ModelData::Type);
    auto model = modelData.model.get();

    // Create a new LLama context from the model
    auto ctx_params = llama_context_default_params();
    ctx_params.n_ctx = _contextSize.get().payload.intValue;
    _data->ctx = std::shared_ptr<llama_context>(llama_init_from_model(model, ctx_params), llama_free);
    if (!_data->ctx) {
      throw ActivationError("Failed to create chat context");
    }

    // Keep a reference to the model data and create a mutex
    _data->_modelData = input;
    _data->_mutex = std::make_shared<std::mutex>();
    _data->n_threads = _threads.get().payload.intValue;

    auto &mmproj = _mmproj.get();
    if (mmproj.valueType != SHType::None) {
      // Load the multimodal projector if provided
      auto mmproj_path = SHSTRING_PREFER_SHSTRVIEW(mmproj);
      if (!mmproj_path.empty()) {
        _data->mmproj_path = mmproj_path;
        _data->clip_ctx = clip_model_load(mmproj_path.c_str(), 0);
        if (!_data->clip_ctx) {
          throw ActivationError("Failed to load CLIP model");
        }
      }
    }

    return ObjectVar.Get(_data);
  }
};

// Add text to the conversation
struct ChatAddBos {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::AnyType; }

  PARAM_PARAMVAR(_chat, "Chat", "The chat context to add text to", {Chat::VarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_chat));

  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  void activate(SHContext *context, const SHVar &input) {
    auto &chatData = varAsObjectChecked<ChatData>(_chat.get(), Chat::Type);
    std::lock_guard<std::mutex> lock(*chatData._mutex);

    auto model = llama_get_model(chatData.ctx.get());
    auto vocab = llama_model_get_vocab(model);
    auto bos = llama_vocab_bos(vocab);

    // Get the input text
    common_batch_add(chatData.batch, bos, chatData.n_past++, {0}, false);

    // Process the batch
    if (llama_decode(chatData.ctx.get(), chatData.batch)) {
      throw ActivationError("Failed to decode input");
    }
  }
};

// Add text to the conversation
struct ChatAddText {
  ChatAddText() {
    _logitsLast = Var(false);
    _ignoreSpecial = Var(false);
  }

  static SHTypesInfo inputTypes() { return shards::CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::StringType; }

  PARAM_PARAMVAR(_chat, "Chat", "The chat context to add text to", {Chat::VarType});
  PARAM_PARAMVAR(_logitsLast, "NeedLogits",
                 "Whether to add logits for the last token (true) or not (false), this is needed to prepare for generation",
                 {shards::CoreInfo::BoolType, shards::CoreInfo::BoolVarType});
  PARAM_PARAMVAR(_prefixTokens, "PrefixTokens", "Optional raw tokens to add to the beginning",
                 {shards::CoreInfo::NoneType, shards::CoreInfo::IntSeqType, shards::CoreInfo::IntVarSeqType});
  PARAM_PARAMVAR(_suffixTokens, "SuffixTokens", "Optional raw tokens to add to the end",
                 {shards::CoreInfo::NoneType, shards::CoreInfo::IntSeqType, shards::CoreInfo::IntVarSeqType});
  PARAM_PARAMVAR(_ignoreSpecial, "IgnoreSpecial", "Whether to ignore special tokens (true) or not (false)",
                 {shards::CoreInfo::BoolType, shards::CoreInfo::BoolVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_chat), PARAM_IMPL_FOR(_logitsLast), PARAM_IMPL_FOR(_prefixTokens), PARAM_IMPL_FOR(_suffixTokens),
             PARAM_IMPL_FOR(_ignoreSpecial));

  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);
    _text = {};
  }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  std::string _text;

  void activate(SHContext *context, const SHVar &input) {
    auto &chatData = varAsObjectChecked<ChatData>(_chat.get(), Chat::Type);
    std::lock_guard<std::mutex> lock(*chatData._mutex);

    // Get the input text
    _text.clear();
    auto view = SHSTRVIEW(input);
    _text.assign(view.data(), view.size());

    // Tokenize the input
    auto parse_special = !_ignoreSpecial.get().payload.boolValue;
    llama_tokens tokens = common_tokenize(chatData.ctx.get(), _text, false, parse_special);

    // Get max batch size
    auto n_batch = size_t(chatData.params.n_batch);
    if (n_batch <= 0) {
      throw ActivationError("Invalid batch size");
    }

    // Process prefix tokens if provided
    if (_prefixTokens.get().valueType != SHType::None) {
      auto prefixTokens = _prefixTokens.get().payload.seqValue;
      // Process in batches
      common_batch_clear(chatData.batch);
      size_t batchCount = 0;

      for (size_t i = 0; i < prefixTokens.len; i++) {
        common_batch_add(chatData.batch, prefixTokens.elements[i].payload.intValue, chatData.n_past++, {0}, false);
        batchCount++;

        // Process the batch if we've reached the max size
        if (batchCount == n_batch) {
          if (llama_decode(chatData.ctx.get(), chatData.batch)) {
            throw ActivationError("Failed to decode prefix tokens");
          }
          common_batch_clear(chatData.batch);
          batchCount = 0;
        }
      }

      // Process any remaining tokens
      if (batchCount > 0) {
        if (llama_decode(chatData.ctx.get(), chatData.batch)) {
          throw ActivationError("Failed to decode prefix tokens");
        }
      }
    }

    // Process text tokens in batches that don't exceed n_batch
    for (size_t i = 0; i < tokens.size(); i += n_batch) {
      common_batch_clear(chatData.batch);
      for (size_t j = 0; j < n_batch && i + j < tokens.size(); j++) {
        bool is_last = (i + j == tokens.size() - 1) && (_suffixTokens.get().valueType == SHType::None) &&
                       (_logitsLast.get().payload.boolValue);
        common_batch_add(chatData.batch, tokens[i + j], chatData.n_past++, {0}, is_last);
      }

      // Process the batch
      if (llama_decode(chatData.ctx.get(), chatData.batch)) {
        throw ActivationError("Failed to decode input tokens");
      }
    }

    // Process suffix tokens if provided
    if (_suffixTokens.get().valueType != SHType::None) {
      auto suffixTokens = _suffixTokens.get().payload.seqValue;
      // Process in batches
      common_batch_clear(chatData.batch);
      size_t batchCount = 0;

      for (size_t i = 0; i < suffixTokens.len; i++) {
        bool is_last = (i == suffixTokens.len - 1) && (_logitsLast.get().payload.boolValue);
        common_batch_add(chatData.batch, suffixTokens.elements[i].payload.intValue, chatData.n_past++, {0}, is_last);
        batchCount++;

        // Process the batch if we've reached the max size
        if (batchCount == n_batch) {
          if (llama_decode(chatData.ctx.get(), chatData.batch)) {
            throw ActivationError("Failed to decode suffix tokens");
          }
          common_batch_clear(chatData.batch);
          batchCount = 0;
        }
      }

      // Process any remaining tokens
      if (batchCount > 0) {
        if (llama_decode(chatData.ctx.get(), chatData.batch)) {
          throw ActivationError("Failed to decode suffix tokens");
        }
      }
    }
  }
};

// Add an image to the conversation
struct ChatAddImage {
  ChatAddImage() {
    _embeddings = Var(256);
    _logitsLast = Var(false);
  }

  static SHTypesInfo inputTypes() { return shards::CoreInfo::ImageType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::ImageType; }

  PARAM_PARAMVAR(_chat, "Chat", "The chat context to add the image to", {Chat::VarType});
  PARAM_PARAMVAR(_logitsLast, "NeedLogits",
                 "Whether to add logits for the last token (true) or not (false), this is needed to prepare for generation",
                 {shards::CoreInfo::BoolType, shards::CoreInfo::BoolVarType});
  PARAM_PARAMVAR(_embeddings, "ImageTokens", "Maximum number of tokens to use for image representation in the context window",
                 {shards::CoreInfo::IntType, shards::CoreInfo::IntVarType});
  PARAM_PARAMVAR(_prefixTokens, "PrefixTokens", "Optional raw tokens to add to the beginning",
                 {shards::CoreInfo::NoneType, shards::CoreInfo::IntSeqType, shards::CoreInfo::IntVarSeqType});
  PARAM_PARAMVAR(_suffixTokens, "SuffixTokens", "Optional raw tokens to add to the end",
                 {shards::CoreInfo::NoneType, shards::CoreInfo::IntSeqType, shards::CoreInfo::IntVarSeqType});
  PARAM_IMPL(PARAM_IMPL_FOR(_chat), PARAM_IMPL_FOR(_logitsLast), PARAM_IMPL_FOR(_embeddings), PARAM_IMPL_FOR(_prefixTokens),
             PARAM_IMPL_FOR(_suffixTokens));

  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  void activate(SHContext *context, const SHVar &input) {
    auto &image = input.payload.imageValue;
    if (image->channels != 3) {
      throw ActivationError("Image must have 3 channels");
    }

    auto &chatData = varAsObjectChecked<ChatData>(_chat.get(), Chat::Type);

    // Check if we have the CLIP model loaded
    if (!chatData.clip_ctx) {
      throw ActivationError("Chat has no CLIP model loaded - can't process images");
    }

    std::lock_guard<std::mutex> lock(*chatData._mutex);

    // Get the model for embedding dimensions
    auto model = llama_get_model(chatData.ctx.get());
    const int n_embd = llama_model_n_embd(model);
    const int n_ubatch = llama_n_ubatch(chatData.ctx.get());

    // Calculate the maximum tokens we'll allow for the image
    // This is limited by both the user-specified ImageTokens parameter and the context size
    const int max_tokens = std::min((int)_embeddings.get().payload.intValue, n_ubatch);

    // Load the image
    struct clip_image_u8 *img_u8 = clip_image_u8_init();
    DEFER({ clip_image_u8_free(img_u8); });
    clip_build_img_from_pixels(image->data, image->width, image->height, img_u8);

    // Preprocess the image
    clip_image_f32_batch batch_f32{};
    DEFER({ clip_image_f32_batch_free(&batch_f32); });
    auto ok = clip_image_preprocess(chatData.clip_ctx, img_u8, &batch_f32);
    if (!ok) {
      throw ActivationError("Failed to preprocess image");
    }

    // Calculate the actual number of image patches based on the image and patch size
    const int patch_size = clip_get_patch_size(chatData.clip_ctx);
    // Get dimensions post-preprocessing (should be square as per CLIP preprocessing)
    int image_size = clip_get_image_size(chatData.clip_ctx);
    // Calculate the number of patches (this is the actual number of tokens CLIP will produce)
    int actual_n_patches = (image_size / patch_size) * (image_size / patch_size);
    // Add 1 for the class embedding token (common in CLIP models)
    actual_n_patches += 1;

    // Allocate space for the full embedding output from CLIP
    // We need to allocate the full size that CLIP might write
    std::vector<float> image_embd_v;
    image_embd_v.resize(actual_n_patches * n_embd);

    // Encode the image
    ok = clip_image_batch_encode(chatData.clip_ctx, chatData.n_threads, &batch_f32, image_embd_v.data());
    if (!ok) {
      throw ActivationError("Failed to encode image");
    }

    // Ensure we don't exceed our maximum token count for the LLM
    int actual_n_tokens = std::min(actual_n_patches, max_tokens);

    // Prefix tokens if provided
    if (_prefixTokens.get().valueType != SHType::None) {
      common_batch_clear(chatData.batch);
      auto prefixTokens = _prefixTokens.get().payload.seqValue;
      for (auto &t : prefixTokens) {
        common_batch_add(chatData.batch, t.payload.intValue, chatData.n_past++, {0}, false);
      }
      // Process the batch
      if (llama_decode(chatData.ctx.get(), chatData.batch)) {
        throw ActivationError("Failed to decode input");
      }
    }

    // Process the image embeddings
    {
      llama_set_causal_attn(chatData.ctx.get(), false);
      DEFER({ llama_set_causal_attn(chatData.ctx.get(), true); });

      // Use the actual token count for the embedding batch
      decode_embd_batch batch_img(image_embd_v.data(), actual_n_tokens, chatData.n_past, 0);

      if (_suffixTokens.get().valueType == SHType::None && _logitsLast.get().payload.boolValue) {
        batch_img.batch.logits[batch_img.batch.n_tokens - 1] = true;
      }

      // Process the batch
      if (llama_decode(chatData.ctx.get(), batch_img.batch)) {
        throw ActivationError("Failed to decode image");
      }
      chatData.n_past += actual_n_tokens;
    }

    // Suffix tokens if provided
    if (_suffixTokens.get().valueType != SHType::None) {
      common_batch_clear(chatData.batch);
      auto suffixTokens = _suffixTokens.get().payload.seqValue;
      for (auto &t : suffixTokens) {
        common_batch_add(chatData.batch, t.payload.intValue, chatData.n_past++, {0}, false);
      }

      // Add logits for the last token if it's user input (for continuing with generation)
      if (_logitsLast.get().payload.boolValue) {
        chatData.batch.logits[chatData.batch.n_tokens - 1] = true;
      }

      // Process the batch
      if (llama_decode(chatData.ctx.get(), chatData.batch)) {
        throw ActivationError("Failed to decode input");
      }
    }
  }
};

// Generate text from the conversation
struct ChatGenerate {
  static SHTypesInfo inputTypes() { return Chat::Type; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::StringType; }

  ChatGenerate() {
    _maxTokens = Var(512);
    // as per llama default
    _temperature = Var(0.80f);
    _topP = Var(0.95f);
    _minP = Var(0.05f);
  }

  PARAM_PARAMVAR(_maxTokens, "MaxTokens", "Maximum number of tokens to generate",
                 {shards::CoreInfo::IntType, shards::CoreInfo::IntVarType});
  PARAM_PARAMVAR(_temperature, "Temperature", "Sampling temperature",
                 {shards::CoreInfo::FloatType, shards::CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_topP, "TopP", "Top-p sampling threshold", {shards::CoreInfo::FloatType, shards::CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_minP, "MinP", "Min-p sampling threshold", {shards::CoreInfo::FloatType, shards::CoreInfo::FloatVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_maxTokens), PARAM_IMPL_FOR(_temperature), PARAM_IMPL_FOR(_topP), PARAM_IMPL_FOR(_minP));

  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);
    _output.clear();
  }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  // Helper to check if a token is a special token
  bool is_special_token(const llama_token token, const llama_vocab *vocab) {
    // Check if the token is an end-of-generation token or other special marker
    return llama_vocab_is_eog(vocab, token);
  }

  std::string _output;

  SHVar activate(SHContext *context, const SHVar &input) {
    // TODO, Wrap in awaitne and support cancellation!

    _output.clear();
    auto &chatData = varAsObjectChecked<ChatData>(input, Chat::Type);
    std::lock_guard<std::mutex> lock(*chatData._mutex);

    if (!chatData.ctx) {
      throw ActivationError("Chat context is not initialized");
    }

    // Get the max tokens to generate
    int n_predict = _maxTokens.get().payload.intValue;

    // Set up sampling parameters
    common_params params{};
    params.sampling.temp = _temperature.get().payload.floatValue;
    params.sampling.top_p = _topP.get().payload.floatValue;
    params.sampling.min_p = _minP.get().payload.floatValue;

    // Create the sampler
    auto model = llama_get_model(chatData.ctx.get());
    auto vocab = llama_model_get_vocab(model);
    common_sampler *sampling_ctx = common_sampler_init(model, params.sampling);
    DEFER({ common_sampler_free(sampling_ctx); });

    // Ensure logits are enabled for the last token before generation
    if (chatData.n_past > 0) {
      // Check if logits were computed for the last token in context
      auto *logits = llama_get_logits(chatData.ctx.get());
      if (!logits) {
        throw ActivationError("Logits not available for the last token - make sure to set the NeedLogits parameter to true in "
                              "your last AddText/AddImage call");
      }
    } else {
      throw ActivationError("Context is empty - add some text before generating");
    }

    // Generate tokens
    chatData.is_generating = true;
    for (int i = 0; i < n_predict && chatData.is_generating; i++) {
      llama_token token_id = common_sampler_sample(sampling_ctx, chatData.ctx.get(), -1);
      common_sampler_accept(sampling_ctx, token_id, true);

      // Check for special tokens that indicate end of generation
      if (llama_vocab_is_eog(vocab, token_id)) {
        break;
      }

      // Convert token to string
      std::string piece = common_token_to_piece(chatData.ctx.get(), token_id);
      _output += piece;

      common_batch_clear(chatData.batch);
      common_batch_add(chatData.batch, token_id, chatData.n_past++, {0}, true);

      if (llama_decode(chatData.ctx.get(), chatData.batch)) {
        throw ActivationError("Failed to decode token during generation");
      }
    }

    chatData.is_generating = false;
    return Var(_output);
  }
};

// Reset the conversation history
struct ChatReset {
  static SHTypesInfo inputTypes() { return Chat::Type; }
  static SHTypesInfo outputTypes() { return Chat::Type; }

  void cleanup(SHContext *context) {}

  void warmup(SHContext *context) {}

  SHTypeInfo compose(SHInstanceData &data) { return outputTypes().elements[0]; }

  void activate(SHContext *context, const SHVar &input) {
    auto &chatData = varAsObjectChecked<ChatData>(input, Chat::Type);
    std::lock_guard<std::mutex> lock(*chatData._mutex);

    // Reset the context
    chatData.n_past = 0;
    llama_kv_self_clear(chatData.ctx.get());
  }
};
} // namespace llm

SHARDS_REGISTER_FN(llm_chat) {
  REGISTER_SHARD("LLM.Chat", llm::Chat);
  REGISTER_SHARD("LLM.AddText", llm::ChatAddText);
  REGISTER_SHARD("LLM.AddImage", llm::ChatAddImage);
  REGISTER_SHARD("LLM.Generate", llm::ChatGenerate);
  REGISTER_SHARD("LLM.Reset", llm::ChatReset);
  REGISTER_SHARD("LLM.AddBos", llm::ChatAddBos);
}
} // namespace shards