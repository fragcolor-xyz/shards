#include "shared.hpp"

#include "../../../deps/llama.cpp/examples/llava/clip-impl.h"
#include "../../../deps/llama.cpp/examples/llava/mtmd.h"
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

  ChatData() {
    SHLOG_DEBUG("ChatData constructor called");

    batch = llama_batch_init(params.n_batch, 0, 1);
  }

  // CLIP model components
  struct clip_ctx *clip_ctx = nullptr;
  std::string mmproj_path;
  int n_threads = 1;

  // State tracking
  llama_pos n_past = 0;

  ~ChatData() {
    SHLOG_DEBUG("ChatData destructor called");

    if (clip_ctx) {
      clip_free(clip_ctx);
      clip_ctx = nullptr;
    }
    llama_batch_free(batch);
  }
};

// Helper for handling embedded images
struct decode_embd_batch {
  int n_pos_per_embd = 1;
  int n_mmproj_embd = 0;
  std::vector<llama_pos> pos;
  std::vector<llama_pos> pos_view; // used by mrope
  std::vector<int32_t> n_seq_id;
  std::vector<llama_seq_id> seq_id_0;
  std::vector<llama_seq_id *> seq_ids;
  std::vector<int8_t> logits;
  llama_batch batch;

  // Constructor with support for M-RoPE and multiple position dimensions
  decode_embd_batch(float *embd, int32_t n_tokens, int n_pos_per_embd, int n_mmproj_embd)
      : n_pos_per_embd(n_pos_per_embd), n_mmproj_embd(n_mmproj_embd) {
    pos.resize(n_tokens * n_pos_per_embd);
    n_seq_id.resize(n_tokens);
    seq_ids.resize(n_tokens + 1);
    logits.resize(n_tokens);
    seq_id_0.resize(1);
    seq_ids[n_tokens] = nullptr;
    batch = {
        /*n_tokens       =*/n_tokens,
        /*tokens         =*/nullptr,
        /*embd           =*/embd,
        /*pos            =*/pos.data(),
        /*n_seq_id       =*/n_seq_id.data(),
        /*seq_id         =*/seq_ids.data(),
        /*logits         =*/logits.data(),
    };
  }

  // Set positions for standard (non-M-RoPE) models
  void set_position_normal(llama_pos pos_0, llama_seq_id seq_id) {
    seq_id_0[0] = seq_id;
    for (int i = 0; i < batch.n_tokens; i++) {
      batch.pos[i] = pos_0 + i;
      batch.n_seq_id[i] = 1;
      batch.seq_id[i] = seq_id_0.data();
      batch.logits[i] = false;
    }
  }

  // Set positions for M-RoPE models (used by Qwen2VL)
  void set_position_mrope(llama_pos pos_0, int nx, int ny, llama_seq_id seq_id) {
    if (n_pos_per_embd != 4) {
      throw std::runtime_error("M-RoPE requires 4 position dimensions");
    }
    seq_id_0[0] = seq_id;
    for (int y = 0; y < ny; y++) {
      for (int x = 0; x < nx; x++) {
        int i = y * nx + x;
        pos[i] = pos_0;
        pos[i + batch.n_tokens] = pos_0 + y;
        pos[i + batch.n_tokens * 2] = pos_0 + x;
        pos[i + batch.n_tokens * 3] = 0; // last pos dim is unused
      }
    }
    for (int i = 0; i < batch.n_tokens; i++) {
      batch.n_seq_id[i] = 1;
      batch.seq_id[i] = seq_id_0.data();
      batch.logits[i] = false;
    }
  }

  // Get a view of a subset of the batch
  llama_batch get_view(int offset, int n_tokens) {
    llama_pos *pos_ptr;
    pos_view.clear();
    pos_view.resize(n_tokens * n_pos_per_embd);
    if (n_pos_per_embd > 1) {
      // mrope
      // for example, with layout of src: 1234...1234...1234...1234...
      //       offset 2 will give us dst: 34...34...34...34...
      for (int i = 0; i < n_pos_per_embd; i++) {
        auto src = pos.begin() + i * batch.n_tokens + offset;
        pos_view.insert(pos_view.end(), src, src + n_tokens);
      }
      pos_ptr = pos_view.data();
    } else {
      // normal
      pos_ptr = pos.data() + offset;
    }
    return {
        /*n_tokens       =*/n_tokens,
        /*tokens         =*/nullptr,
        /*embd           =*/batch.embd + offset * n_mmproj_embd,
        /*pos            =*/pos_ptr,
        /*n_seq_id       =*/batch.n_seq_id + offset,
        /*seq_id         =*/batch.seq_id + offset,
        /*logits         =*/batch.logits + offset,
    };
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
      SHLOG_DEBUG("Releasing existing ChatData, refcount: {}", ObjectVar.GetRefCount(_data));

      ObjectVar.Release(_data);
      _data = nullptr;
    }
  }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    // Extract the LLM model from input
    auto &modelData = varAsObjectChecked<ModelData>(input, ModelData::Type);
    auto model = modelData.model.get();

    if (_data) {
      SHLOG_DEBUG("Releasing existing ChatData, refcount: {}", ObjectVar.GetRefCount(_data));

      ObjectVar.Release(_data);
      _data = nullptr;
    }
    _data = ObjectVar.New();

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
  static SHTypesInfo inputTypes() { return Chat::Type; }
  static SHTypesInfo outputTypes() { return Chat::Type; }

  void activate(SHContext *context, const SHVar &input) {
    auto &chatData = varAsObjectChecked<ChatData>(input, Chat::Type);
    std::lock_guard<std::mutex> lock(*chatData._mutex);

    auto model = llama_get_model(chatData.ctx.get());
    auto vocab = llama_model_get_vocab(model);
    auto bos = llama_vocab_bos(vocab);

    // Get the input text
    common_batch_clear(chatData.batch);
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
    const int n_ubatch = llama_n_ubatch(chatData.ctx.get());

    // Calculate the maximum tokens we'll allow for the image
    // This is limited by both the user-specified ImageTokens parameter and the context size
    const int max_tokens = std::min((int)_embeddings.get().payload.intValue, n_ubatch);

    // Create a mtmd_bitmap from the image data
    mtmd_bitmap bitmap;
    bitmap.nx = image->width;
    bitmap.ny = image->height;
    bitmap.data.resize(image->width * image->height * 3);
    std::memcpy(bitmap.data.data(), image->data, bitmap.data.size());

    // Create a mtmd_context from the CLIP model
    mtmd_context_params ctx_params{};
    ctx_params.use_gpu = true; // Use GPU if available
    ctx_params.print_timings = false;
    ctx_params.n_threads = chatData.n_threads;
    ctx_params.verbosity = GGML_LOG_LEVEL_INFO;

    // Create a temporary mtmd_context for this operation
    mtmd_context *ctx_vision = mtmd_init_from_file(chatData.mmproj_path.c_str(), model, ctx_params);
    if (!ctx_vision) {
      throw ActivationError("Failed to initialize vision context");
    }
    DEFER({ mtmd_free(ctx_vision); });

    // Create input text and chunks for tokenization
    mtmd_input_text text;
    text.text = "<__image__>"; // Just the image marker
    text.add_special = false;
    text.parse_special = true;

    std::vector<mtmd_bitmap> bitmaps = {bitmap};
    mtmd_input_chunks chunks;

    // Tokenize the input with the image
    if (mtmd_tokenize(ctx_vision, chunks, text, bitmaps) != 0) {
      throw ActivationError("Failed to tokenize image input");
    }

    // Find the image chunk
    mtmd_input_chunk *image_chunk = nullptr;
    for (auto &chunk : chunks) {
      if (chunk.type == MTMD_INPUT_CHUNK_TYPE_IMAGE) {
        image_chunk = &chunk;
        break;
      }
    }

    if (!image_chunk || !image_chunk->tokens_image) {
      throw ActivationError("No image tokens found after tokenization");
    }

    // Get the number of tokens in the image
    size_t n_image_tokens = mtmd_image_tokens_get_n_tokens(image_chunk->tokens_image.get());

    // Ensure we don't exceed our maximum token count for the LLM
    int actual_n_tokens = std::min((int)n_image_tokens, max_tokens);

    // Prefix tokens if provided
    if (_prefixTokens.get().valueType != SHType::None) {
      common_batch_clear(chatData.batch);
      auto prefixTokens = _prefixTokens.get().payload.seqValue;
      for (auto &t : prefixTokens) {
        common_batch_add(chatData.batch, t.payload.intValue, chatData.n_past++, {0}, false);
      }
      // Process the batch
      if (llama_decode(chatData.ctx.get(), chatData.batch)) {
        throw ActivationError("Failed to decode prefix tokens");
      }
    }

    // Encode the image
    if (mtmd_encode(ctx_vision, image_chunk->tokens_image.get()) != 0) {
      throw ActivationError("Failed to encode image");
    }

    // Get the embeddings
    float *image_embd = mtmd_get_output_embd(ctx_vision);

    // Process the image embeddings
    {
      // Check if we need to use non-causal attention for this model
      bool use_non_causal = mtmd_decode_use_non_causal(ctx_vision);
      if (use_non_causal) {
        llama_set_causal_attn(chatData.ctx.get(), false);
      }
      DEFER({
        if (use_non_causal) {
          llama_set_causal_attn(chatData.ctx.get(), true);
        }
      });

      // Determine if we need to use M-RoPE positions
      bool use_mrope = mtmd_decode_use_mrope(ctx_vision);
      int n_pos_per_embd = use_mrope ? 4 : 1;
      int n_mmproj_embd = clip_n_mmproj_embd(chatData.clip_ctx);

      // Create embedding batch with proper positioning
      decode_embd_batch batch_img(image_embd, actual_n_tokens, n_pos_per_embd, n_mmproj_embd);

      // Set positions based on whether we're using M-RoPE or not
      if (use_mrope) {
        size_t nx = mtmd_image_tokens_get_nx(image_chunk->tokens_image.get());
        size_t ny = mtmd_image_tokens_get_ny(image_chunk->tokens_image.get());
        batch_img.set_position_mrope(chatData.n_past, nx, ny, 0);
      } else {
        batch_img.set_position_normal(chatData.n_past, 0);
      }

      // Set logits for the last token if needed
      if (_suffixTokens.get().valueType == SHType::None && _logitsLast.get().payload.boolValue) {
        batch_img.batch.logits[batch_img.batch.n_tokens - 1] = true;
      }

      // Process the batch
      if (llama_decode(chatData.ctx.get(), batch_img.batch)) {
        throw ActivationError("Failed to decode image");
      }

      // Update n_past based on whether we're using M-RoPE or not
      // For M-RoPE, the whole image counts as a single position
      llama_pos n_pos = mtmd_image_tokens_get_n_pos(image_chunk->tokens_image.get());
      chatData.n_past += n_pos;
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
        throw ActivationError("Failed to decode suffix tokens");
      }
    }
  }
};

// Generate text from the conversation
struct ChatGenerate {
  static SHTypesInfo inputTypes() { return Chat::Type; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::StringType; }

  ChatGenerate() {
    // as per llama.cpp default
    _temperature = Var(0.80f);
    _topP = Var(0.95f);
    _minP = Var(0.05f);
  }

  PARAM_PARAMVAR(_temperature, "Temperature", "Sampling temperature",
                 {shards::CoreInfo::FloatType, shards::CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_topP, "TopP", "Top-p sampling threshold", {shards::CoreInfo::FloatType, shards::CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_minP, "MinP", "Min-p sampling threshold", {shards::CoreInfo::FloatType, shards::CoreInfo::FloatVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_temperature), PARAM_IMPL_FOR(_topP), PARAM_IMPL_FOR(_minP));

  void cleanup(SHContext *context) {
    if (sampling_ctx) {
      common_sampler_free(sampling_ctx);
      sampling_ctx = nullptr;
    }

    _output = {};

    PARAM_CLEANUP(context);
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

  common_sampler *sampling_ctx = nullptr;

  SHVar activate(SHContext *context, const SHVar &input) {
    auto &chatData = varAsObjectChecked<ChatData>(input, Chat::Type);
    std::lock_guard<std::mutex> lock(*chatData._mutex);

    if (!chatData.ctx) {
      throw ActivationError("Chat context is not initialized");
    }

    auto model = llama_get_model(chatData.ctx.get());
    auto vocab = llama_model_get_vocab(model);

    if (!sampling_ctx) {
      // Set up sampling parameters
      common_params params{};
      params.sampling.temp = _temperature.get().payload.floatValue;
      params.sampling.top_p = _topP.get().payload.floatValue;
      params.sampling.min_p = _minP.get().payload.floatValue;

      // Create the sampler
      sampling_ctx = common_sampler_init(model, params.sampling);

      // Ensure logits are enabled for the last token before generation
      // Notice this will still ABORT on relwithdebinfo...
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
    }

    _output.clear();

    // Generate a token
    llama_token token_id = common_sampler_sample(sampling_ctx, chatData.ctx.get(), -1);
    common_sampler_accept(sampling_ctx, token_id, true);

    // Check for special tokens that indicate end of generation
    if (llama_vocab_is_eog(vocab, token_id)) {
      // free the sampler, we start fresh from next call
      common_sampler_free(sampling_ctx);
      sampling_ctx = nullptr;
      // output the empty string to flag end of generation
      return Var(_output);
    }

    // Convert token to string
    _output = common_token_to_piece(chatData.ctx.get(), token_id);

    common_batch_clear(chatData.batch);
    common_batch_add(chatData.batch, token_id, chatData.n_past++, {0}, true);

    if (llama_decode(chatData.ctx.get(), chatData.batch)) {
      throw ActivationError("Failed to decode token during generation");
    }

    return Var(_output);
  }
};

// Reset the conversation history
struct ChatReset {
  static SHTypesInfo inputTypes() { return Chat::Type; }
  static SHTypesInfo outputTypes() { return Chat::Type; }

  void activate(SHContext *context, const SHVar &input) {
    auto &chatData = varAsObjectChecked<ChatData>(input, Chat::Type);
    std::lock_guard<std::mutex> lock(*chatData._mutex);

    // Reset the context
    chatData.n_past = 0;
    llama_kv_self_clear(chatData.ctx.get());
  }
};

struct ChatTemplate {
  static SHTypesInfo inputTypes() { return Chat::Type; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::StringType; }

  std::string _template;

  void cleanup(SHContext *context) { _template = {}; }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto &chatData = varAsObjectChecked<ChatData>(input, Chat::Type);
    std::lock_guard<std::mutex> lock(*chatData._mutex);

    auto model = llama_get_model(chatData.ctx.get());
    auto template_ = llama_model_chat_template(model, NULL);
    _template = std::string(template_);

    return Var(_template);
  }
};

struct ChatBos {
  static SHTypesInfo inputTypes() { return Chat::Type; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::StringType; }

  std::string _output;

  void cleanup(SHContext *context) { _output = {}; }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto &chatData = varAsObjectChecked<ChatData>(input, Chat::Type);
    std::lock_guard<std::mutex> lock(*chatData._mutex);

    auto model = llama_get_model(chatData.ctx.get());
    auto vocab = llama_model_get_vocab(model);
    auto bos = llama_vocab_bos(vocab);
    _output = common_token_to_piece(chatData.ctx.get(), bos);

    return Var(_output);
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
  REGISTER_SHARD("LLM.Template", llm::ChatTemplate);
  REGISTER_SHARD("LLM.Bos", llm::ChatBos);
}
} // namespace shards