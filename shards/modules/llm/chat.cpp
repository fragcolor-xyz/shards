#include "shared.hpp"

// Try to include the CLIP library for image support
#if __has_include("../../../deps/llama.cpp/examples/llava/clip.h")
#define HAS_CLIP_SUPPORT
#include "../../../deps/llama.cpp/examples/llava/clip.h"
#else
#pragma message("CLIP support is not available - building LLM.Chat with text-only support")
#endif

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

#ifdef HAS_CLIP_SUPPORT
  // CLIP model components
  struct clip_ctx *clip_ctx = nullptr;
  std::string mmproj_path;
  int n_threads = 1;
#endif

  // State tracking
  llama_pos n_past = 0;
  bool is_generating = false;

  ~ChatData() {
#ifdef HAS_CLIP_SUPPORT
    if (clip_ctx) {
      clip_free(clip_ctx);
      clip_ctx = nullptr;
    }
#endif
  }
};

// Helper for handling embedded images
#ifdef HAS_CLIP_SUPPORT
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
#endif

// The MultiModal Chat shard
struct Chat {
  static inline int32_t ObjectId = 'llch';
  static inline const char VariableName[] = "LLM.Chat";
  static inline ::shards::Type Type = ::shards::Type::Object(CoreCC, ObjectId);
  static inline SHTypeInfo RawType = Type;
  static inline ::shards::Type VarType = ::shards::Type::VariableOf(Type);
  static inline shards::ObjectVar<ChatData> ObjectVar{VariableName, RawType.object.vendorId, RawType.object.typeId};

  Chat() { _threads = Var(4); }

  static SHTypesInfo inputTypes() { return ModelData::Type; } // Takes LLM.Model
  static SHTypesInfo outputTypes() { return Type; }

  PARAM_PARAMVAR(_mmproj, "MMProj", "Path to the multimodal projector model",
                 {shards::CoreInfo::NoneType, shards::CoreInfo::StringType, shards::CoreInfo::StringVarType});
  PARAM_PARAMVAR(_threads, "Threads", "Number of threads to use for image processing",
                 {shards::CoreInfo::IntType, shards::CoreInfo::IntVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_mmproj), PARAM_IMPL_FOR(_threads));

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
#ifdef HAS_CLIP_SUPPORT
      // Load the multimodal projector if provided
      auto mmproj_path = SHSTRING_PREFER_SHSTRVIEW(mmproj);
      if (!mmproj_path.empty()) {
        _data->mmproj_path = mmproj_path;
        int verbosity = 1;
        struct clip_context_params params = {.use_gpu = true, .verbosity = verbosity};
        _data->clip_ctx = clip_init(mmproj_path.c_str(), params);
        if (!_data->clip_ctx) {
          throw ActivationError("Failed to load CLIP model");
        }
      }
#else
      auto mmproj_path = SHSTRING_PREFER_SHSTRVIEW(mmproj);
      if (!mmproj_path.empty()) {
        SHLOG_DEBUG("CLIP support is not available - images will not be processed");
      }
#endif
    }

    // Reset conversation context
    const auto vocab = llama_model_get_vocab(model);
    const auto bos_token = llama_vocab_bos(vocab);

    auto batch = llama_batch_init(1, 0, 1);
    batch.n_tokens = 1;
    batch.token[0] = bos_token;
    batch.pos[0] = 0;
    batch.seq_id[0][0] = 0;
    batch.n_seq_id[0] = 1;
    batch.logits[0] = false;

    if (llama_decode(_data->ctx.get(), batch)) {
      throw ActivationError("Failed to initialize chat with BOS token");
    }
    _data->n_past = 1;

    return ObjectVar.Get(_data);
  }
};

// Add text to the conversation
struct ChatAddText {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::BoolType; }

  PARAM_PARAMVAR(_chat, "Chat", "The chat context to add text to", {Chat::VarType});
  PARAM_PARAMVAR(_isUser, "IsUser", "Whether this is user input (true) or system message (false)",
                 {shards::CoreInfo::BoolType, shards::CoreInfo::BoolVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_chat), PARAM_IMPL_FOR(_isUser));

  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);
    _tokensCache = {};
  }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  std::vector<llama_token> _tokensCache;

  SHVar activate(SHContext *context, const SHVar &input) {
    auto &chatData = varAsObjectChecked<ChatData>(_chat.get(), Chat::Type);
    std::lock_guard<std::mutex> lock(*chatData._mutex);

    const auto vocab = llama_model_get_vocab(llama_get_model(chatData.ctx.get()));

    // Get the input text
    auto text = SHSTRVIEW(input);

    // Format based on whether it's user or system
    std::string formatted;
    if (_isUser.get().payload.boolValue) {
      formatted = "<start_of_turn>user\n" + std::string(text) + "<end_of_turn>";
    } else {
      formatted = "<start_of_turn>model\n" + std::string(text) + "<end_of_turn>";
    }

    // Tokenize the input
    _tokensCache.resize(formatted.size());
    auto nTokens =
        llama_tokenize(vocab, formatted.data(), formatted.size(), _tokensCache.data(), _tokensCache.size(), true, true);
    if (nTokens < 0) {
      throw ActivationError("Failed to tokenize input");
    }

    // Prepare the batch
    auto batch = llama_batch_init(_tokensCache.size(), 0, 1);
    for (size_t i = 0; i < _tokensCache.size(); i++) {
      batch.token[i] = _tokensCache[i];
      batch.pos[i] = chatData.n_past + i;
      batch.seq_id[i][0] = 0;
      batch.n_seq_id[i] = 1;
      batch.logits[i] = false;
    }
    batch.n_tokens = _tokensCache.size();

    // Add logits for the last token if it's user input (for continuing with generation)
    if (_isUser.get().payload.boolValue && _tokensCache.size() > 0) {
      batch.logits[batch.n_tokens - 1] = true;
    }

    // Process the batch
    if (llama_decode(chatData.ctx.get(), batch)) {
      llama_batch_free(batch);
      return Var(false);
    }

    // Update position counter
    chatData.n_past += _tokensCache.size();
    llama_batch_free(batch);
    return Var(true);
  }
};

// // Add an image to the conversation
// struct ChatAddImage {
//   static SHTypesInfo inputTypes() { return shards::CoreInfo::StringType; } // Path to image
//   static SHTypesInfo outputTypes() { return shards::CoreInfo::BoolType; }

//   PARAM_PARAMVAR(_chat, "Chat", "The chat context to add the image to", {Chat::VarType});
//   PARAM_IMPL(PARAM_IMPL_FOR(_chat));

//   void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

//   void warmup(SHContext *context) { PARAM_WARMUP(context); }

//   PARAM_REQUIRED_VARIABLES();
//   SHTypeInfo compose(SHInstanceData &data) {
//     PARAM_COMPOSE_REQUIRED_VARIABLES(data);
//     return outputTypes().elements[0];
//   }

//   SHVar activate(SHContext *context, const SHVar &input) {
// #ifdef HAS_CLIP_SUPPORT
//     auto &chatData = varAsObjectChecked<ChatData>(_chat.get(), Chat::Type);

//     // Check if we have the CLIP model loaded
//     if (!chatData.clip_ctx) {
//       throw ActivationError("Chat has no CLIP model loaded - can't process images");
//     }

//     std::lock_guard<std::mutex> lock(*chatData._mutex);

//     // Get the image path
//     std::string image_path = SHSTRING_PREFER_SHSTRVIEW(input);

//     // Get the model for embedding dimensions
//     auto model = llama_get_model(chatData.ctx.get());
//     const int n_embd = llama_model_n_embd(model);
//     const int n_tokens = 256; // Standard for Gemma3

//     // Allocate space for embeddings
//     std::vector<float> image_embd_v;
//     image_embd_v.resize(n_tokens * n_embd);

//     // Load the image
//     struct clip_image_u8 *img_u8 = clip_image_u8_init();
//     bool ok = clip_image_load_from_file(image_path.c_str(), img_u8);
//     if (!ok) {
//       clip_image_u8_free(img_u8);
//       throw ActivationError("Failed to load image: " + image_path);
//     }

//     // Preprocess the image
//     clip_image_f32_batch batch_f32;
//     ok = clip_image_preprocess(chatData.clip_ctx, img_u8, &batch_f32);
//     if (!ok) {
//       clip_image_f32_batch_free(&batch_f32);
//       clip_image_u8_free(img_u8);
//       throw ActivationError("Failed to preprocess image");
//     }

//     // Encode the image
//     ok = clip_image_batch_encode(chatData.clip_ctx, chatData.n_threads, &batch_f32, image_embd_v.data());
//     if (!ok) {
//       clip_image_f32_batch_free(&batch_f32);
//       clip_image_u8_free(img_u8);
//       throw ActivationError("Failed to encode image");
//     }

//     // Free image resources
//     clip_image_f32_batch_free(&batch_f32);
//     clip_image_u8_free(img_u8);

//     // Start image sequence
//     const auto vocab = llama_model_get_vocab(model);
//     std::vector<llama_token> start_tokens = llama_tokenize(vocab, "<start_of_image>", 16, true, true);

//     // Create batch for start token
//     auto batch = llama_batch_init(start_tokens.size(), 0, 1);
//     for (size_t i = 0; i < start_tokens.size(); i++) {
//       llama_batch_add(batch, start_tokens[i], chatData.n_past + i, {0}, false);
//     }

//     // Process the start token
//     if (llama_decode(chatData.ctx.get(), batch)) {
//       llama_batch_free(batch);
//       return Var(false);
//     }
//     chatData.n_past += start_tokens.size();
//     llama_batch_free(batch);

//     // Process the image embeddings
//     llama_set_causal_attn(chatData.ctx.get(), false);
//     decode_embd_batch batch_img(image_embd_v.data(), n_tokens, chatData.n_past, 0);
//     if (llama_decode(chatData.ctx.get(), batch_img.batch)) {
//       return Var(false);
//     }
//     chatData.n_past += n_tokens;
//     llama_set_causal_attn(chatData.ctx.get(), true);

//     // End image sequence
//     std::vector<llama_token> end_tokens = llama_tokenize(vocab, "<end_of_image>", 14, true, true);
//     batch = llama_batch_init(end_tokens.size(), 0, 1);
//     for (size_t i = 0; i < end_tokens.size(); i++) {
//       llama_batch_add(batch, end_tokens[i], chatData.n_past + i, {0}, false);
//     }

//     if (llama_decode(chatData.ctx.get(), batch)) {
//       llama_batch_free(batch);
//       return Var(false);
//     }

//     chatData.n_past += end_tokens.size();
//     llama_batch_free(batch);

//     return Var(true);
// #else
//     throw ActivationError("CLIP support is not available - cannot process images");
// #endif
//   }
// };

// Generate text from the conversation
struct ChatGenerate {
  static SHTypesInfo inputTypes() { return Chat::Type; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::StringType; }

  ChatGenerate() {
    _maxTokens = Var(512);
    // as per llama default
    _temperature = Var(0.80f);
    _topP = Var(0.95f);
  }

  PARAM_PARAMVAR(_maxTokens, "MaxTokens", "Maximum number of tokens to generate",
                 {shards::CoreInfo::IntType, shards::CoreInfo::IntVarType});
  PARAM_PARAMVAR(_temperature, "Temperature", "Sampling temperature",
                 {shards::CoreInfo::FloatType, shards::CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_topP, "TopP", "Top-p sampling threshold", {shards::CoreInfo::FloatType, shards::CoreInfo::FloatVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_maxTokens), PARAM_IMPL_FOR(_temperature), PARAM_IMPL_FOR(_topP));

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

    // Create the sampler
    auto model = llama_get_model(chatData.ctx.get());
    auto vocab = llama_model_get_vocab(model);
    common_sampler *sampling_ctx = common_sampler_init(model, params.sampling);

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

      // Decode the new token
      auto batch = llama_batch_init(1, 0, 1);
      batch.n_tokens = 1;
      batch.token[0] = token_id;
      batch.pos[0] = chatData.n_past++;
      batch.seq_id[0][0] = 0;
      batch.n_seq_id[0] = 1;
      batch.logits[0] = true;

      if (llama_decode(chatData.ctx.get(), batch)) {
        llama_batch_free(batch);
        common_sampler_free(sampling_ctx);
        throw ActivationError("Failed to decode token during generation");
      }

      llama_batch_free(batch);
    }

    chatData.is_generating = false;
    common_sampler_free(sampling_ctx);
    return Var(_output);
  }
};

// // Stop generation if it's in progress
// struct ChatStopGeneration {
//   static SHTypesInfo inputTypes() { return Chat::Type; }
//   static SHTypesInfo outputTypes() { return Chat::Type; }

//   void cleanup(SHContext *context) {}

//   void warmup(SHContext *context) {}

//   SHTypeInfo compose(SHInstanceData &data) { return outputTypes().elements[0]; }

//   SHVar activate(SHContext *context, const SHVar &input) {
//     auto &chatData = varAsObjectChecked<ChatData>(input, Chat::Type);
//     std::lock_guard<std::mutex> lock(*chatData._mutex);

//     chatData.is_generating = false;
//     return Var();
//   }
// };

// Reset the conversation history
struct ChatReset {
  static SHTypesInfo inputTypes() { return Chat::Type; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::BoolType; }

  void cleanup(SHContext *context) {}

  void warmup(SHContext *context) {}

  SHTypeInfo compose(SHInstanceData &data) { return outputTypes().elements[0]; }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto &chatData = varAsObjectChecked<ChatData>(input, Chat::Type);
    std::lock_guard<std::mutex> lock(*chatData._mutex);

    // Reset the context
    chatData.n_past = 0;
    llama_kv_self_clear(chatData.ctx.get());

    // Reinitialize with BOS token
    const auto model = llama_get_model(chatData.ctx.get());
    const auto vocab = llama_model_get_vocab(model);
    const auto bos_token = llama_vocab_bos(vocab);

    auto batch = llama_batch_init(1, 0, 1);
    batch.n_tokens = 1;
    batch.token[0] = bos_token;
    batch.pos[0] = 0;
    batch.seq_id[0][0] = 0;
    batch.n_seq_id[0] = 1;
    batch.logits[0] = false;

    if (llama_decode(chatData.ctx.get(), batch)) {
      llama_batch_free(batch);
      return Var(false);
    }

    chatData.n_past = 1;
    llama_batch_free(batch);
    return Var(true);
  }
};
} // namespace llm

SHARDS_REGISTER_FN(llm_chat) {
  REGISTER_SHARD("LLM.Chat", llm::Chat);
  REGISTER_SHARD("LLM.AddText", llm::ChatAddText);
  // REGISTER_SHARD("LLM.Chat.AddImage", llm::ChatAddImage);
  REGISTER_SHARD("LLM.Generate", llm::ChatGenerate);
  // REGISTER_SHARD("LLM.Chat.StopGeneration", llm::ChatStopGeneration);
  REGISTER_SHARD("LLM.Reset", llm::ChatReset);
}
} // namespace shards