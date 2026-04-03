# ML Module (shards-ml)

This module provides all ML/AI inference shards. It combines two subsystems:

1. **Tensor & embedding shards** — candle-based tensor operations, BERT embeddings, tokenization
2. **LLM shards** — full LLM inference via mistral.rs (text, vision, audio, multimodal)

## Architecture

### Dependencies

```
mistralrs 0.8.1          — LLM inference engine (BlockingModel API)
candle-core 0.10.2       — tensor operations (same version mistral.rs uses internally)
candle-nn 0.10.2         — neural network layers (for BERT)
candle-transformers 0.10.2 — pre-built model architectures (BERT)
tokenizers 0.21.0        — HuggingFace tokenizers
image 0.25               — image format conversion for LLM.AddImage
tokio 1                  — async runtime for GGUF/UQFF builders
```

Previously this module used a forked candle from `shards-lang/candle.git`. That fork was replaced with crates.io candle 0.10.2 as part of the mistral.rs migration (PR #1262).

### Feature Flags

| Feature | What it enables | CMake trigger |
|---------|----------------|---------------|
| `metal` | Apple GPU (macOS/iOS/visionOS) | `APPLE && !DISABLE_CANDLE_METAL` |
| `cuda` | NVIDIA GPU | `ENABLE_CUDA && CUDAToolkit found` |
| `accelerate` | Apple Accelerate BLAS | `APPLE` |

Features flow from CMakeLists.txt → Cargo via `add_rust_library(... FEATURES ...)`.

### Source Files

| File | Purpose |
|------|---------|
| `lib.rs` | Module registration, global Metal/CUDA device selection, Tensor type |
| `llm.rs` | LLM shards: Model, Chat, AddText, AddImage, AddAudio, Generate, Reset |
| `model.rs` | BERT model loading (ML.Model) and forward pass (ML.Forward) |
| `tensor.rs` | 16 tensor operation shards (Mul, Add, Reshape, etc.) |
| `tokenizer.rs` | ML.Tokenizer, ML.Tokens, ML.Detokenize |
| `umap.rs` | Tensor.UMAP dimensionality reduction |

## LLM Shards (llm.rs)

### Object Types

- **`LLMModel`** (fourCC: `aiMD`) — wraps `mistralrs::blocking::BlockingModel`. The BlockingModel owns its own tokio runtime internally. Stored as ref-counted object.
- **`LLMChat`** (fourCC: `aiCH`) — wraps a model reference + message history (`Vec<ChatMessage>`). The model reference is a `ClonedVar` that properly maintains the LLMModel's refcount via `cloneVar`/`destroyVar`.

### Shards

#### LLM.Model
Load any model from HuggingFace or local path. Auto-detects architecture.

```shards
; Auto-detect with ISQ quantization (downloads full model, quantizes on load)
"google/gemma-4-E2B-it" | LLM.Model(ISQ: ISQBits::Four) = model

; GGUF pre-quantized (fast load, but limited architecture support)
"unsloth/Qwen3-0.6B-GGUF" | LLM.Model(Files: "Qwen3-0.6B-Q4_K_M.gguf") = model

; UQFF pre-quantized (mistral.rs native format, fast load, full architecture support)
"mistralrs-community/gemma-4-E2B-it-UQFF" | LLM.Model(UQFF: "q4k-0.uqff") = model
```

**Parameters:**
- `ISQ` — In-situ quantization (None/Two/Four/Eight). Only for auto-detect path.
- `Files` — GGUF filename(s). Switches to GGUF loader.
- `UQFF` — UQFF filename. Switches to UQFF loader (recommended for pre-quantized).
- `Embedding` — When true, loads as an embedding model for use with LLM.Embed. Uses F32 dtype on CPU to avoid F16 NaN issues.

**Loading paths:**
1. **Auto-detect** (no Files/UQFF): Downloads safetensors from HF, auto-detects architecture, optionally applies ISQ. Slowest for first load but supports everything.
2. **GGUF** (Files param): Loads pre-quantized GGUF. Fast but limited architectures in mistral.rs 0.8.1 (no Gemma — only Llama, Qwen, Phi, Mistral, etc.).
3. **UQFF** (UQFF param): Loads mistral.rs native pre-quantized format. Fast and supports all architectures. Recommended when available.

#### LLM.Chat
Creates a chat session. Stores message history internally.

```shards
model | LLM.Chat = chat
```

#### LLM.AddText
Adds a text message to chat history. Passthrough — returns the input string.

```shards
"What is 2+2?" | LLM.AddText(Chat: chat Role: ChatRole::User)
```

#### LLM.AddImage
Adds an image to chat history. Requires vision-capable model (e.g. Gemma 4). Converts SHImage (RGB/RGBA/grayscale, 8-bit) to DynamicImage for mistral.rs.

```shards
LoadImage("photo.png") | LLM.AddImage(Chat: chat Text: "Describe this image.")
```

#### LLM.AddAudio
Adds audio samples to chat history. Requires audio-capable model (e.g. Gemma 4 E2B/E4B).

```shards
audio-samples | LLM.AddAudio(Chat: chat Text: "Transcribe this." SampleRate: 16000)
```

#### LLM.Generate
Runs inference and returns generated text. Auto-appends assistant reply to chat history for multi-turn.

```shards
chat | LLM.Generate(Temperature: 0.1 MaxTokens: 64) | Log
```

#### LLM.Reset
Clears chat message history.

```shards
chat | LLM.Reset
```

#### LLM.Embed
Generate text embeddings using an embedding model. The model must be loaded with `Embedding: true`.

```shards
"google/embeddinggemma-300m" | LLM.Model(Embedding: true) = model
"What is graphene?" | LLM.Embed(Model: model) ; outputs [Float] (768-dim for embeddinggemma-300m)
```

Supported embedding models include `google/embeddinggemma-300m`, `Qwen/Qwen3-Embedding-0.6B`, and any model supported by mistral.rs's `EmbeddingModelBuilder`. Uses F32 dtype on CPU to avoid F16 NaN issues.

### Internal Design Notes

**BlockingModel lifetime:** The `BlockingModel` from mistral.rs owns a tokio runtime. It MUST NOT be created inside an existing tokio context (panics). Since shard `activate()` is always called from the C++ runtime's synchronous thread, this is safe. For GGUF/UQFF paths, we create the tokio runtime manually via `tokio::runtime::Builder` since `BlockingModel::from_builder` only accepts `TextModelBuilder`/`ModelBuilder`.

**Chat message storage:** Messages are stored as `Vec<ChatMessage>` where `ChatMessage` is an enum with `Text`, `Image`, and `Audio` variants. On each `LLM.Generate` call, messages are rebuilt into a `RequestBuilder`. Image and audio data is cloned during this rebuild (once per generation call). The `RequestBuilder` also carries sampling parameters (temperature, top_p, max_tokens).

**ref_counted_object_type_impl macro conflict:** Each invocation of this macro generates module-level statics (`TYPE_OBJECT_NAME`, `TYPE_OBJECT_INFO`). Two invocations in the same module conflict. Solution: wrap each object type in its own inner module (`mod model_obj`, `mod chat_obj`).

## Known Limitations & Gotchas

### Performance
- **CPU-only inference is slow for vision/audio.** Text generation on small models (0.6B-2B) is fine, but processing images through the vision encoder on CPU takes minutes. CUDA or Metal GPU is needed for production multimodal use.
- **ISQ quantization on first load is slow.** Downloads full F16 model (~10GB for Gemma 4 E2B) then quantizes to Q4. Use UQFF pre-quantized models to avoid this.
- **candle CPU kernels are slower than llama.cpp's.** llama.cpp has hand-tuned AVX2/NEON SIMD kernels; candle is pure Rust. The tradeoff is broader model support and Rust safety.

### Model Compatibility
- **GGUF architecture support is limited in mistral.rs 0.8.1.** Only: Llama, Qwen2, Qwen3, Phi2, Phi3, Starcoder2, Mistral3, Mamba, Falcon, etc. No Gemma in GGUF. Use safetensors auto-detect or UQFF for Gemma.
- **Gated models require HF token.** Google Gemma 4 is gated. Set `HF_TOKEN` env var or run `hf login`. The `mistralrs-community/*-UQFF` variants are ungated.
- **Embeddings are async-only in mistral.rs.** `BlockingModel` doesn't expose `generate_embeddings`. An `LLM.Embed` shard would need to access the inner async model via `inner()` and `rt.block_on()`.

### visionOS Metal Support
candle 0.10.2 uses `objc2-metal` which supports visionOS, but needs cfg condition patches:
- mistral.rs PR #1911 adds `target_os = "visionos"` to Metal cfg conditions
- Until merged, fork mistral.rs or cherry-pick the ~10-line fix
- Remove `-DDISABLE_CANDLE_METAL=ON` from `.github/workflows/build-ios.yml` visionOS build after fix

### C++ LLM Module (Disabled)
The old C++ module at `shards/modules/llm/` is disabled but not deleted. It provided:
- `LLM.Model`, `LLM.Chat`, `LLM.AddText`, `LLM.AddImage`, `LLM.Generate` (via llama.cpp)
- `Whisper.Load`, `Whisper.Transcribe` (via whisper.cpp)
- `LLM.Context`, `LLM.Embed`, `LLM.Tokenize`, `LLM.Detokenize`

The Rust shards now override these names. The C++ module and its llama.cpp/whisper.cpp deps in `deps/CMakeLists.txt` are both disabled via `if(FALSE ...)` guards. Delete entirely once the Rust shards reach full parity.

## Test Files

| Test | Model | What it tests | CI? |
|------|-------|---------------|-----|
| `llm-mistral.shs` | SmolLM2-135M (safetensors) | Text generation, ISQ Q4 | Yes |
| `llm-gguf.shs` | Qwen3-0.6B (GGUF) | GGUF loading, text gen | Yes |
| `llm-gemma4.shs` | Gemma 4 E2B (safetensors, gated) | Gated model + ISQ | No (needs HF_TOKEN) |
| `llm-multimodal.shs` | Gemma 4 E2B UQFF (ungated) | Vision + multi-turn + reset | No (slow on CPU) |
| `llm-embed.shs` | embeddinggemma-300m | Text embeddings (768-dim) | Yes |
| `ml.shs` / `ml-test.shs` | BERT (safetensors) | Tensor ops, embeddings | Yes |
| `whisper.shs` | (placeholder) | Audio transcription | No (needs audio model) |
