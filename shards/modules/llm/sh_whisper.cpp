#include "shared.hpp"

namespace shards {
namespace llm {

struct WhisperData {
  static inline int32_t ObjectId = 'whsp';
  static inline const char VariableName[] = "Whisper.Model";
  static inline ::shards::Type Type = ::shards::Type::Object(CoreCC, ObjectId);
  static inline SHTypeInfo RawType = Type;
  static inline ::shards::Type VarType = ::shards::Type::VariableOf(Type);
  static inline shards::ObjectVar<WhisperData> ObjectVar{VariableName, RawType.object.vendorId, RawType.object.typeId};

  static inline std::atomic_uint32_t usageCounter;

  WhisperData() {
    SHLOG_DEBUG("WhisperData constructor called");

    uint32_t expected = usageCounter.load(std::memory_order_acquire);
    uint32_t desired;
    do {
      desired = expected + 1;
    } while (!usageCounter.compare_exchange_weak(expected, desired, std::memory_order_release));

    if (desired == 1) {
      SHLOG_DEBUG("Initializing whisper backend");
      // No specific backend initialization needed for whisper
    }
  }

  ~WhisperData() {
    SHLOG_DEBUG("WhisperData destructor called");

    uint32_t prev = usageCounter.fetch_sub(1, std::memory_order_acq_rel);
    if (prev == 1) {
      SHLOG_DEBUG("Freeing whisper backend");
      // No specific backend cleanup needed for whisper
    }
  }

  std::shared_ptr<whisper_context> ctx;
};

struct WhisperLoad {
  WhisperLoad() { _gpu = Var(false); }

  WhisperData *_data{};

  static SHTypesInfo inputTypes() { return shards::CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return WhisperData::Type; }

  PARAM_PARAMVAR(_gpu, "GPU", "If GPU is enabled, use GPU for inference",
                 {shards::CoreInfo::BoolType, shards::CoreInfo::BoolVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_gpu));

  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);
    if (_data) {
      SHLOG_DEBUG("Releasing existing WhisperData, refcount: {}", WhisperData::ObjectVar.GetRefCount(_data));

      WhisperData::ObjectVar.Release(_data);
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
    struct whisper_context_params cparams = whisper_context_default_params();
    cparams.use_gpu = _gpu.get().payload.boolValue;

    if (_data) {
      SHLOG_DEBUG("Releasing existing WhisperData, refcount: {}", WhisperData::ObjectVar.GetRefCount(_data));

      WhisperData::ObjectVar.Release(_data);
      _data = nullptr;
    }
    _data = WhisperData::ObjectVar.New();

    auto path = SHSTRING_PREFER_SHSTRVIEW(input); // to ensure null termination
    _data->ctx = std::shared_ptr<whisper_context>(whisper_init_from_file_with_params(path.c_str(), cparams), whisper_free);

    if (!_data->ctx) {
      throw ActivationError(fmt::format("Failed to load whisper model from {}", path));
    }

    return WhisperData::ObjectVar.Get(_data);
  }
};

struct WhisperTranscribe {
  static SHTypesInfo inputTypes() { return shards::CoreInfo::FloatSeqType; }
  static SHTypesInfo outputTypes() { return shards::CoreInfo::StringType; }

  WhisperTranscribe() {
    _language = Var("auto");
    _translate = Var(false);
    _threads = Var(4);
    _beamSize = Var(-1);
    _maxTokens = Var(32);
    _printTimestamps = Var(false);
  }

  PARAM_PARAMVAR(_model, "Model", "The whisper model to use", {WhisperData::VarType});
  PARAM_PARAMVAR(_language, "Language", "Language code (e.g., 'en', 'auto' for auto-detect)",
                 {shards::CoreInfo::StringType, shards::CoreInfo::StringVarType});
  PARAM_PARAMVAR(_translate, "Translate", "Translate to English", {shards::CoreInfo::BoolType, shards::CoreInfo::BoolVarType});
  PARAM_PARAMVAR(_threads, "Threads", "Number of threads to use", {shards::CoreInfo::IntType, shards::CoreInfo::IntVarType});
  PARAM_PARAMVAR(_beamSize, "BeamSize", "Beam size for beam search (-1 for greedy)",
                 {shards::CoreInfo::IntType, shards::CoreInfo::IntVarType});
  PARAM_PARAMVAR(_maxTokens, "MaxTokens", "Maximum number of tokens per audio chunk",
                 {shards::CoreInfo::IntType, shards::CoreInfo::IntVarType});
  PARAM_PARAMVAR(_printTimestamps, "PrintTimestamps", "Include timestamps in output",
                 {shards::CoreInfo::BoolType, shards::CoreInfo::BoolVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_model), PARAM_IMPL_FOR(_language), PARAM_IMPL_FOR(_translate), PARAM_IMPL_FOR(_threads),
             PARAM_IMPL_FOR(_beamSize), PARAM_IMPL_FOR(_maxTokens), PARAM_IMPL_FOR(_printTimestamps));

  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);
    _result = "";
  }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  PARAM_REQUIRED_VARIABLES();
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  std::string _result;

  SHVar activate(SHContext *context, const SHVar &input) {
    auto &data = varAsObjectChecked<WhisperData>(_model.get(), WhisperData::Type);
    auto ctx = data.ctx.get();

    if (!ctx) {
      throw ActivationError("Whisper context is null");
    }

    // Convert input sequence to float array
    std::vector<float> samples;
    samples.reserve(input.payload.seqValue.len);
    for (uint32_t i = 0; i < input.payload.seqValue.len; i++) {
      samples.push_back(input.payload.seqValue.elements[i].payload.floatValue);
    }

    // Set up parameters for transcription
    whisper_full_params wparams = whisper_full_default_params(_beamSize.get().payload.intValue > 1 ? WHISPER_SAMPLING_BEAM_SEARCH
                                                                                                   : WHISPER_SAMPLING_GREEDY);

    wparams.print_progress = false;
    wparams.print_special = false;
    wparams.print_realtime = false;
    wparams.print_timestamps = _printTimestamps.get().payload.boolValue;
    wparams.translate = _translate.get().payload.boolValue;
    wparams.language = _language.get().payload.stringValue;
    wparams.n_threads = _threads.get().payload.intValue;
    wparams.beam_search.beam_size = _beamSize.get().payload.intValue;
    wparams.max_tokens = _maxTokens.get().payload.intValue;

    // Run the transcription
    if (whisper_full(ctx, wparams, samples.data(), samples.size()) != 0) {
      throw ActivationError("Failed to process audio with whisper");
    }

    // Collect the results
    _result.clear();
    const int n_segments = whisper_full_n_segments(ctx);

    for (int i = 0; i < n_segments; ++i) {
      const char *text = whisper_full_get_segment_text(ctx, i);

      if (wparams.print_timestamps) {
        const int64_t t0 = whisper_full_get_segment_t0(ctx, i);
        const int64_t t1 = whisper_full_get_segment_t1(ctx, i);

        // Format timestamp as [hh:mm:ss.ms --> hh:mm:ss.ms]
        auto format_timestamp = [](int64_t ms) {
          int64_t hours = ms / (1000 * 60 * 60);
          ms -= hours * (1000 * 60 * 60);

          int64_t minutes = ms / (1000 * 60);
          ms -= minutes * (1000 * 60);

          int64_t seconds = ms / 1000;
          ms -= seconds * 1000;

          return fmt::format("{:02d}:{:02d}:{:02d}.{:03d}", hours, minutes, seconds, ms);
        };

        _result += fmt::format("[{} --> {}] {}", format_timestamp(t0), format_timestamp(t1), text);
      } else {
        _result += text;
      }
    }

    return Var(_result);
  }
};

} // namespace llm

SHARDS_REGISTER_FN(whisper) {
  REGISTER_SHARD("Whisper.Load", llm::WhisperLoad);
  REGISTER_SHARD("Whisper.Transcribe", llm::WhisperTranscribe);
}

} // namespace shards