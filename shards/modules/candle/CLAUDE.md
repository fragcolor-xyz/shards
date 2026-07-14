# ML Module (shards-ml)

This module provides all ML/AI inference shards. It combines two subsystems:

1. **Tensor & embedding shards** — candle-based tensor operations, BERT embeddings, tokenization
2. **AI shards** — full LLM inference via mistral.rs (text, vision, audio, multimodal)

## Architecture

### Dependencies

```
mistralrs 0.8.1          — LLM inference engine (async Model API)
candle-core 0.10.2       — tensor operations (same version mistral.rs uses internally)
candle-nn 0.10.2         — neural network layers (for BERT)
candle-transformers 0.10.2 — pre-built model architectures (BERT)
tokenizers 0.21.0        — HuggingFace tokenizers
image 0.25               — image format conversion for AI.AddImage
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
| `llm.rs` | AI shards: Model, Chat, AddText, AddImage, AddAudio, Generate, Reset |
| `model.rs` | BERT model loading (ML.Model) and forward pass (ML.Forward) |
| `muscriptor.rs` | MuScriptor.Load / MuScriptor.Transcribe / MuScriptor.ToMidi (audio → MIDI) |
| `tensor.rs` | 16 tensor operation shards (Mul, Add, Reshape, etc.) |
| `tokenizer.rs` | ML.Tokenizer, ML.Tokens, ML.Detokenize |
| `umap.rs` | Tensor.UMAP dimensionality reduction |

## LLM Shards (llm.rs)

### Object Types

- **`LLMModel`** (fourCC: `aiMD`) — wraps `Arc<mistralrs::Model>`. The Model is used via `run_future` + `TOKIO_RUNTIME` for async inference. Stored as ref-counted object.
- **`LLMChat`** (fourCC: `aiCH`) — wraps a model reference + message history (`Vec<ChatMessage>`). The model reference is a `ClonedVar` that properly maintains the LLMModel's refcount via `cloneVar`/`destroyVar`.

### Shards

#### AI.Model
Load any model from HuggingFace or local path. Auto-detects architecture.

```shards
; Auto-detect with ISQ quantization (downloads full model, quantizes on load)
"google/gemma-4-E2B-it" | AI.Model(ISQ: ISQBits::Four) = model

; GGUF pre-quantized (fast load, but limited architecture support)
"unsloth/Qwen3-0.6B-GGUF" | AI.Model(Files: "Qwen3-0.6B-Q4_K_M.gguf") = model

; UQFF pre-quantized (mistral.rs native format, fast load, full architecture support)
"mistralrs-community/gemma-4-E2B-it-UQFF" | AI.Model(UQFF: "q4k-0.uqff") = model
```

**Parameters:**
- `ISQ` — In-situ quantization (None/Two/Four/Eight). Only for auto-detect path.
- `Files` — GGUF filename(s). Switches to GGUF loader.
- `UQFF` — UQFF filename. Switches to UQFF loader (recommended for pre-quantized).
- `Embedding` — When true, loads as an embedding model for use with AI.Embed. Uses F32 dtype on CPU to avoid F16 NaN issues.

**Loading paths:**
1. **Auto-detect** (no Files/UQFF): Downloads safetensors from HF, auto-detects architecture, optionally applies ISQ. Slowest for first load but supports everything.
2. **GGUF** (Files param): Loads pre-quantized GGUF. Fast but limited architectures in mistral.rs 0.8.1 (no Gemma — only Llama, Qwen, Phi, Mistral, etc.).
3. **UQFF** (UQFF param): Loads mistral.rs native pre-quantized format. Fast and supports all architectures. Recommended when available.

#### AI.Chat
Creates a chat session. Stores message history internally.

```shards
model | AI.Chat = chat
```

#### AI.AddText
Adds a text message to chat history. Passthrough — returns the input string.

```shards
"What is 2+2?" | AI.AddText(Chat: chat Role: ChatRole::User)
```

#### AI.AddImage
Adds an image to chat history. Requires vision-capable model (e.g. Gemma 4). Converts SHImage (RGB/RGBA/grayscale, 8-bit) to DynamicImage for mistral.rs.

```shards
LoadImage("photo.png") | AI.AddImage(Chat: chat Text: "Describe this image.")
```

#### AI.AddAudio
Adds audio samples to chat history. Requires audio-capable model (e.g. Gemma 4 E2B/E4B).

```shards
audio-samples | AI.AddAudio(Chat: chat Text: "Transcribe this." SampleRate: 16000)
```

#### AI.Generate
Runs inference and returns generated text. Auto-appends assistant reply to chat history for multi-turn.

```shards
chat | AI.Generate(Temperature: 0.1 MaxTokens: 64) | Log
```

#### AI.Reset
Clears chat message history.

```shards
chat | AI.Reset
```

#### AI.Embed
Generate text embeddings using an embedding model. The model must be loaded with `Embedding: true`.

```shards
"google/embeddinggemma-300m" | AI.Model(Embedding: true) = model
"What is graphene?" | AI.Embed(Model: model) ; outputs [Float] (768-dim for embeddinggemma-300m)
```

Supported embedding models include `google/embeddinggemma-300m`, `Qwen/Qwen3-Embedding-0.6B`, and any model supported by mistral.rs's `EmbeddingModelBuilder`. Uses F32 dtype on CPU to avoid F16 NaN issues.

### CRITICAL: Async Pattern for Rust Shards

**NEVER use `block_on`, `BlockingModel`, or any thread-blocking call inside `activate()`.** Shards uses coroutine-based concurrency — blocking a thread stalls the entire wire scheduler.

**Required pattern:** Use `shards::core::run_future(context, async { ... }, on_cancel)` with a global `TOKIO_RUNTIME`. This suspends the shards coroutine (yields to the scheduler) while async work runs on a tokio thread pool.

**Canonical example:** `shards/modules/http/src/lib.rs` — study this before writing any async Rust shard.

```rust
// Global runtime (shared across all shard instances)
lazy_static! {
  static ref TOKIO_RUNTIME: Arc<Mutex<tokio::runtime::Runtime>> = Arc::new(Mutex::new(
    tokio::runtime::Builder::new_multi_thread()
      .worker_threads(4)
      .enable_all()
      .build()
      .expect("Failed to create Tokio runtime")
  ));
}

// In activate():
let cancel_token = CancellationToken::new();
let result = run_future(context, async move {
  let runtime = TOKIO_RUNTIME.clone();
  let task = {
    let runtime = runtime.lock().unwrap();
    runtime.spawn(async move {
      // actual async work here (model inference, HTTP, etc.)
    })
  };
  task.await.map_err(|e| FastError::from(e.to_string()))?
}, || { cancel_token.cancel(); });
```

**For mistral.rs:** Use the async `Model` directly (not `BlockingModel`). Spawn inference on the tokio runtime via `run_future`.

**For pure candle ops** (e.g. quantized BERT forward pass): If the operation is fast (<50ms), synchronous in `activate()` is acceptable. For heavy ops (model loading, large batch), wrap in `run_future` anyway.

### Internal Design Notes

**Chat message storage:** Messages are stored as `Vec<ChatMessage>` where `ChatMessage` is an enum with `Text`, `Image`, and `Audio` variants. On each `AI.Generate` call, messages are rebuilt into a `RequestBuilder`. Image and audio data is cloned during this rebuild (once per generation call). The `RequestBuilder` also carries sampling parameters (temperature, top_p, max_tokens).

**ref_counted_object_type_impl macro conflict:** Each invocation of this macro generates module-level statics (`TYPE_OBJECT_NAME`, `TYPE_OBJECT_INFO`). Two invocations in the same module conflict. Solution: wrap each object type in its own inner module (`mod model_obj`, `mod chat_obj`).

## Known Limitations & Gotchas

### Performance
- **CPU-only inference is slow for vision/audio.** Text generation on small models (0.6B-2B) is fine, but processing images through the vision encoder on CPU takes minutes. CUDA or Metal GPU is needed for production multimodal use.
- **ISQ quantization on first load is slow.** Downloads full F16 model (~10GB for Gemma 4 E2B) then quantizes to Q4. Use UQFF pre-quantized models to avoid this.
- **candle CPU kernels are slower than llama.cpp's.** llama.cpp has hand-tuned AVX2/NEON SIMD kernels; candle is pure Rust. The tradeoff is broader model support and Rust safety.

### Model Compatibility
- **GGUF architecture support is limited in mistral.rs 0.8.1.** Only: Llama, Qwen2, Qwen3, Phi2, Phi3, Starcoder2, Mistral3, Mamba, Falcon, etc. No Gemma in GGUF. Use safetensors auto-detect or UQFF for Gemma.
- **Gated models require HF token.** Google Gemma 4 is gated. Set `HF_TOKEN` env var or run `hf login`. The `mistralrs-community/*-UQFF` variants are ungated.
- **Embeddings use the async Model.** `AI.Embed` calls `model.generate_embeddings()` via `run_future` + `TOKIO_RUNTIME`, consistent with all other inference shards.

### visionOS Metal Support
candle 0.10.2 uses `objc2-metal` which supports visionOS. Metal is enabled for visionOS builds (no `DISABLE_CANDLE_METAL` flag needed).

### C++ LLM Module (Re-enabled)
The C++ module at `shards/modules/llm/` has been re-enabled, gated on `LLM_ENABLED`. It provides:
- `LLM.Context`, `LLM.Chat`, `LLM.Generate` (via llama.cpp, with Vulkan GPU support)
- `Whisper.Load`, `Whisper.Transcribe` (via whisper.cpp)

The Rust `AI.*` shards (mistral.rs) and C++ `LLM.*` shards (llama.cpp) coexist as a three-tier architecture: AI.* for broad model support, LLM.* for Vulkan GPU acceleration, ML.* for tensor ops.

## MuScriptor Shards (muscriptor.rs)

Audio-to-MIDI music transcription via `candle_transformers::models::muscriptor` (lives in the shards-lang/candle fork, branch `shards-0.11`). Pure candle — no mistralrs/tokio, so these shards are NOT gated on the `llm` feature and exist on every target. Heavy work (load, generate) runs through `run_blocking` (`BlockingShard`), and `MuScriptor.Transcribe` supports mid-generation cancellation via an `AtomicBool` checked in the per-step callback.

```shards
"path/to/model.safetensors" | MuScriptor.Load = model  ; config.json read from alongside, or inferred from weights
samples | MuScriptor.Transcribe(Model: model Instruments: "acoustic_piano,drums") = notes
notes | MuScriptor.ToMidi = midi  ; format-0 SMF bytes
"out.mid" | FS.Write(midi Overwrite: true)
```

- **`MuScriptor.Load`** — safetensors path → model object (fourCC `muSC`). `GPU` param (default true); f16 on Metal, f32 elsewhere. Weights: `hf.co/MuScriptor/muscriptor-{small,medium,large}` (gated: auto, CC BY-NC 4.0).
- **`MuScriptor.Transcribe`** — mono `[Float]` samples → seq of note tables `{pitch: Int, onset: Float, offset: Float, program: Int, drum: Bool, instrument: String}`. Params: `SampleRate` (resamples to 16 kHz via the reference-matching windowed-sinc kernel bank), `Instruments` (string or string-seq conditioning), `BatchSize` (0 = auto: 4 GPU / 1 CPU), `MaxTokens`, `Temperature`/`TopP`/`TopK`/`Seed` (0 temperature = greedy, the reference default). Batch/offline: wants the whole recording, applies the reference's full note cleanup (incl. global same-pitch overlap trimming).
- **`MuScriptor.Stream`** — stateful streaming variant (Markdown.Parse-style: state lives in the shard instance, advanced per activation). Feed `[Float]` samples incrementally from a live/looped wire; buffers at the native rate, transcribes each full 5s segment as it becomes available (all buffered segments batched per activation), and keeps the `TokenDecoder` across activations so the model's tie-prologue carries notes across segment boundaries. Outputs the notes *completed* during that activation (empty while buffering). `Drain: true` (bool-var) flushes the zero-padded partial segment, closes open notes and resets for a new stream; `Reset: true` discards state. Same sampling params as Transcribe; no `BatchSize` (a batch = whatever segments are buffered). Per-note min-duration cleanup only — global overlap trimming needs the whole stream, use Transcribe for offline parity.
- **`MuScriptor.ToMidi`** — note tables → Standard MIDI File (format 0) bytes, byte-for-byte matching the reference `note_event2midi`. Model-free.

The model object wraps `Mutex<Model>` because generation mutates KV-cache state; concurrent transcribes on one model serialize. The sinc resampler and MIDI serializer are ports of `candle-examples/examples/muscriptor/{audio,midi}.rs` (the example crate isn't linkable).

## Test Files

| Test | Model | What it tests | CI? |
|------|-------|---------------|-----|
| `ai-mistral.shs` | SmolLM2-135M (safetensors) | Text generation, ISQ Q4 | Yes |
| `ai-gguf.shs` | Qwen3-0.6B (GGUF) | GGUF loading, text gen | Yes |
| `ai-gemma4.shs` | Gemma 4 E2B (safetensors, gated) | Gated model + ISQ | No (needs HF_TOKEN) |
| `ai-multimodal.shs` | Gemma 4 E2B UQFF (ungated) | Vision + multi-turn + reset | No (slow on CPU) |
| `ai-embed.shs` | embeddinggemma-300m | Text embeddings (768-dim) | No (needs HF_TOKEN) |
| `ml.shs` / `ml-test.shs` | BERT (safetensors) | Tensor ops, embeddings | Yes |
| `whisper.shs` | (placeholder) | Audio transcription | No (needs audio model) |
| `muscriptor.shs` | muscriptor-small (safetensors, gated) | ToMidi golden bytes (always); audio→MIDI transcription when weights present | Golden only |
