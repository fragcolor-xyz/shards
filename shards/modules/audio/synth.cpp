/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2024 Fragcolor Pte. Ltd. */

#include <shards/core/shared.hpp>
#include <shards/core/params.hpp>
#include <miniaudio.h>
#include <algorithm>
#include <cmath>
#include <random>

// Apple Accelerate framework (vForce/vDSP) - preferred on Apple platforms
#ifdef __APPLE__
#include <Accelerate/Accelerate.h>
#define SYNTH_HAS_ACCELERATE 1
#endif

// SIMD headers for non-Apple platforms
#if !defined(SYNTH_HAS_ACCELERATE)
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#elif defined(__AVX2__) || defined(__SSE__)
#include <immintrin.h>
#endif
#endif

// SLEEF for vectorized transcendentals (non-Apple platforms)
#if !defined(SYNTH_HAS_ACCELERATE) && __has_include(<sleef.h>)
#include <sleef.h>
#define SYNTH_HAS_SLEEF 1
#endif

// M_PI is not defined on Windows by default
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace shards {
namespace Audio {

// Forward declare Device and accessor functions from audio.cpp
struct Device;
uint32_t getDeviceBufferSize(void *device);
uint32_t getDeviceSampleRate(void *device);

namespace Synth {

static TableVar experimental{{Var("experimental"), Var(true)}};

// =============================================================================
// SIMD-optimized sin function for audio buffers
// =============================================================================

inline void simdSinf(float *out, const float *in, size_t count) {
#if defined(SYNTH_HAS_ACCELERATE)
  // Apple vForce - highly optimized for Apple Silicon and Intel
  // Process in chunks of INT_MAX to avoid overflow
  constexpr size_t maxChunk = static_cast<size_t>(INT_MAX);
  size_t offset = 0;
  while (offset < count) {
    size_t remaining = count - offset;
    int n = static_cast<int>(std::min(remaining, maxChunk));
    vvsinf(out + offset, in + offset, &n);
    offset += n;
  }
#elif defined(SYNTH_HAS_SLEEF)
  size_t i = 0;
#if defined(__AVX2__)
  for (; i + 8 <= count; i += 8) {
    __m256 va = _mm256_loadu_ps(in + i);
    __m256 vr = Sleef_sinf8_u10avx2(va);
    _mm256_storeu_ps(out + i, vr);
  }
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
  for (; i + 4 <= count; i += 4) {
    float32x4_t va = vld1q_f32(in + i);
    float32x4_t vr = Sleef_sinf4_u10advsimd(va);
    vst1q_f32(out + i, vr);
  }
#endif
  // Scalar fallback for remainder
  for (; i < count; ++i)
    out[i] = std::sin(in[i]);
#else
  // Pure scalar fallback
  for (size_t i = 0; i < count; ++i)
    out[i] = std::sin(in[i]);
#endif
}

// =============================================================================
// Oscillator with FM support and multiple waveforms
// =============================================================================

struct Oscillator {
  enum class Waveform { Sine, Triangle, Sawtooth, Square };
  DECL_ENUM_INFO(Waveform, Waveform, "Type of waveform for the oscillator output.", 'wave');

  // Parameters using PARAM macros
  PARAM_VAR(_waveform, "Waveform", "The waveform type (Sine, Triangle, Sawtooth, Square).", {WaveformEnumInfo::Type});
  PARAM_PARAMVAR(_frequency, "Frequency", "The base frequency in Hz.", {CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_amplitude, "Amplitude", "Output amplitude (0.0 to 1.0).", {CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_index, "Index", "Modulation index - controls FM depth when audio input is provided.",
                 {CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_VAR(_channels, "Channels", "Number of output channels.", {CoreInfo::IntType});
  PARAM_VAR(_sampleRate, "SampleRate", "Sample rate in Hz. Ignored if inside Audio.Channel.", {CoreInfo::IntType});
  PARAM_VAR(_samples, "Samples", "Number of samples per buffer. Ignored if inside Audio.Channel.", {CoreInfo::IntType});

  PARAM_IMPL(PARAM_IMPL_FOR(_waveform), PARAM_IMPL_FOR(_frequency), PARAM_IMPL_FOR(_amplitude), PARAM_IMPL_FOR(_index),
             PARAM_IMPL_FOR(_channels), PARAM_IMPL_FOR(_sampleRate), PARAM_IMPL_FOR(_samples));

  // Internal state
  double _phase{0.0};
  std::vector<float> _buffer;
  std::vector<float> _phaseBuffer;
  std::vector<float> _modPhaseBuffer;

  SHVar *_deviceVar{nullptr};
  Device *_device{nullptr};

  Oscillator() {
    _waveform = Var::Enum(Waveform::Sine, CoreCC, WaveformEnumInfo::TypeId);
    _frequency = Var(440.0);
    _amplitude = Var(1.0);
    _index = Var(1.0);
    _channels = Var(1);
    _sampleRate = Var(44100);
    _samples = Var(1024);
  }

  static SHOptionalString help() {
    return SHCCSTR("Audio oscillator with multiple waveforms and FM synthesis support. "
                   "When input is None, generates a waveform at the specified frequency. "
                   "When input is Audio, uses that signal as phase modulation (FM synthesis). "
                   "Multiple oscillators can be chained for complex FM patches.");
  }

  static inline Types InputTypes{{CoreInfo::NoneType, CoreInfo::AudioType}};
  static SHTypesInfo inputTypes() { return InputTypes; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("None to generate a carrier wave, or Audio to use as phase modulation signal.");
  }
  static SHTypesInfo outputTypes() { return CoreInfo::AudioType; }
  static SHOptionalString outputHelp() { return SHCCSTR("The generated or modulated audio signal."); }

  static const SHTable *properties() { return &experimental.payload.tableValue; }

  PARAM_REQUIRED_VARIABLES();

  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);

    const Waveform waveform = Waveform(_waveform.payload.enumValue);
    const bool isCarrier = (data.inputType.basicType == SHType::None);

    // Select activate function based on input type and waveform (compile-time dispatch)
    if (isCarrier) {
      switch (waveform) {
      case Waveform::Sine:
        OVERRIDE_ACTIVATE(data, activateCarrier<Waveform::Sine>);
        break;
      case Waveform::Triangle:
        OVERRIDE_ACTIVATE(data, activateCarrier<Waveform::Triangle>);
        break;
      case Waveform::Sawtooth:
        OVERRIDE_ACTIVATE(data, activateCarrier<Waveform::Sawtooth>);
        break;
      case Waveform::Square:
        OVERRIDE_ACTIVATE(data, activateCarrier<Waveform::Square>);
        break;
      }
    } else {
      switch (waveform) {
      case Waveform::Sine:
        OVERRIDE_ACTIVATE(data, activateModulated<Waveform::Sine>);
        break;
      case Waveform::Triangle:
        OVERRIDE_ACTIVATE(data, activateModulated<Waveform::Triangle>);
        break;
      case Waveform::Sawtooth:
        OVERRIDE_ACTIVATE(data, activateModulated<Waveform::Sawtooth>);
        break;
      case Waveform::Square:
        OVERRIDE_ACTIVATE(data, activateModulated<Waveform::Square>);
        break;
      }
    }
    return CoreInfo::AudioType;
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);

    _deviceVar = referenceVariable(context, "Audio.Device");
    if (_deviceVar->valueType == SHType::Object) {
      _device = reinterpret_cast<Device *>(_deviceVar->payload.objectValue);
    }

    _phase = 0.0;
  }

  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);

    if (_deviceVar) {
      releaseVariable(_deviceVar);
      _deviceVar = nullptr;
      _device = nullptr;
    }
  }

  // Waveform generation from phase (0 to 2π) - templated for compile-time dispatch
  template <Waveform W> inline float generateSample(float phase) {
    constexpr float TWO_PI = float(2.0 * M_PI);
    // Normalize phase to 0-1 range
    float t = phase / TWO_PI;
    t = t - std::floor(t); // Wrap to [0, 1)

    if constexpr (W == Waveform::Sine) {
      return std::sin(phase);
    } else if constexpr (W == Waveform::Triangle) {
      return 4.0f * std::abs(t - 0.5f) - 1.0f;
    } else if constexpr (W == Waveform::Sawtooth) {
      return 2.0f * t - 1.0f;
    } else if constexpr (W == Waveform::Square) {
      return t < 0.5f ? 1.0f : -1.0f;
    }
  }

  // Vectorized waveform generation - templated for compile-time dispatch
  template <Waveform W> void generateWaveform(float *out, const float *phases, size_t count) {
    if constexpr (W == Waveform::Sine) {
      // Use SIMD-optimized sin for sine waves
      simdSinf(out, phases, count);
    } else {
      // Scalar generation for other waveforms (could add SIMD later)
      for (size_t i = 0; i < count; ++i) {
        out[i] = generateSample<W>(phases[i]);
      }
    }
  }

  // Generate carrier (no modulation input) - templated for compile-time waveform dispatch
  template <Waveform W> SHVar activateCarrier(SHContext *context, const SHVar &input) {
    const ma_uint32 channels = ma_uint32(_channels.payload.intValue);
    ma_uint64 nsamples = ma_uint64(_samples.payload.intValue);
    ma_uint32 sampleRate = ma_uint32(_sampleRate.payload.intValue);

    if (_device) {
      nsamples = getDeviceBufferSize(_device);
      sampleRate = getDeviceSampleRate(_device);
    }

    const double freq = _frequency.get().payload.floatValue;
    const float amp = float(_amplitude.get().payload.floatValue);

    _buffer.resize(channels * nsamples);
    _phaseBuffer.resize(nsamples);

    const double phaseInc = (2.0 * M_PI * freq) / double(sampleRate);

    // Build phase array
    double phase = _phase;
    for (ma_uint64 i = 0; i < nsamples; i++) {
      _phaseBuffer[i] = float(phase);
      phase += phaseInc;
    }
    // Wrap phase to prevent precision loss
    _phase = std::fmod(phase, 2.0 * M_PI);

    // Generate waveform (output to first channel position)
    generateWaveform<W>(_buffer.data(), _phaseBuffer.data(), nsamples);

    // Apply amplitude
    for (ma_uint64 i = 0; i < nsamples; i++) {
      _buffer[i] *= amp;
    }

    // Copy to other channels if needed (planar format)
    for (ma_uint32 ch = 1; ch < channels; ch++) {
      std::memcpy(_buffer.data() + ch * nsamples, _buffer.data(), nsamples * sizeof(float));
    }

    return Var(makeAudio(_buffer.data(), uint32_t(nsamples), sampleRate, uint8_t(channels)));
  }

  // Phase modulation from input audio - templated for compile-time waveform dispatch
  template <Waveform W> SHVar activateModulated(SHContext *context, const SHVar &input) {
    const auto &audio = input.payload.audioValue;
    const uint32_t nsamples = audio.nsamples;
    const uint32_t sampleRate = audioGetSampleRate(audio);
    const double freq = _frequency.get().payload.floatValue;
    const float amp = float(_amplitude.get().payload.floatValue);
    const float modIndex = float(_index.get().payload.floatValue);

    const uint32_t channels = audio.channels;
    _buffer.resize(channels * nsamples);
    _phaseBuffer.resize(nsamples);
    _modPhaseBuffer.resize(nsamples);

    const double phaseInc = (2.0 * M_PI * freq) / double(sampleRate);

    // Build base phase array once (shared across all channels)
    double phase = _phase;
    for (uint32_t i = 0; i < nsamples; i++) {
      _phaseBuffer[i] = float(phase);
      phase += phaseInc;
    }
    // Wrap phase to prevent precision loss
    _phase = std::fmod(phase, 2.0 * M_PI);

    // Process each channel
    for (uint32_t ch = 0; ch < channels; ch++) {
      const float *modSamples = audio.samples + ch * nsamples;
      float *outSamples = _buffer.data() + ch * nsamples;

      // Apply per-channel modulation to the base phase
      for (uint32_t i = 0; i < nsamples; i++) {
        _modPhaseBuffer[i] = _phaseBuffer[i] + modIndex * modSamples[i];
      }

      // Generate waveform
      generateWaveform<W>(outSamples, _modPhaseBuffer.data(), nsamples);

      // Apply amplitude
      for (uint32_t i = 0; i < nsamples; i++) {
        outSamples[i] *= amp;
      }
    }

    return Var(makeAudio(_buffer.data(), nsamples, sampleRate, uint8_t(channels)));
  }

  SHVar activate(SHContext *context, const SHVar &input) { return activateCarrier<Waveform::Sine>(context, input); }
};

// =============================================================================
// Noise Generator
// =============================================================================

struct Noise {
  enum class NoiseType { White, Pink, Brown };
  DECL_ENUM_INFO(NoiseType, NoiseType, "Type of noise to generate.", 'nois');

  // Parameters using PARAM macros
  PARAM_VAR(_type, "Type", "The type of noise to generate (White, Pink, or Brown).", {NoiseTypeEnumInfo::Type});
  PARAM_PARAMVAR(_amplitude, "Amplitude", "Output amplitude (0.0 to 1.0).", {CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_VAR(_channels, "Channels", "Number of output channels.", {CoreInfo::IntType});
  PARAM_VAR(_sampleRate, "SampleRate", "Sample rate in Hz. Ignored if inside Audio.Channel.", {CoreInfo::IntType});
  PARAM_VAR(_samples, "Samples", "Number of samples per buffer. Ignored if inside Audio.Channel.", {CoreInfo::IntType});

  PARAM_IMPL(PARAM_IMPL_FOR(_type), PARAM_IMPL_FOR(_amplitude), PARAM_IMPL_FOR(_channels), PARAM_IMPL_FOR(_sampleRate),
             PARAM_IMPL_FOR(_samples));

  // Internal state
  std::mt19937 _rng;
  std::uniform_real_distribution<float> _dist{-1.0f, 1.0f};

  // Pink noise state (Paul Kellet's algorithm)
  float _pink_b0{0}, _pink_b1{0}, _pink_b2{0}, _pink_b3{0}, _pink_b4{0}, _pink_b5{0}, _pink_b6{0};

  // Brown noise state
  float _brown_last{0};

  std::vector<float> _buffer;

  SHVar *_deviceVar{nullptr};
  Device *_device{nullptr};

  Noise() {
    _type = Var::Enum(NoiseType::White, CoreCC, NoiseTypeEnumInfo::TypeId);
    _amplitude = Var(1.0);
    _channels = Var(1);
    _sampleRate = Var(44100);
    _samples = Var(1024);
  }

  static SHOptionalString help() {
    return SHCCSTR("Generates noise signals. Supports white noise (flat spectrum), pink noise (1/f spectrum), "
                   "and brown/Brownian noise (1/f^2 spectrum).");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString inputHelp() { return DefaultHelpText::InputHelpIgnored; }
  static SHTypesInfo outputTypes() { return CoreInfo::AudioType; }
  static SHOptionalString outputHelp() { return SHCCSTR("The generated noise signal."); }

  static const SHTable *properties() { return &experimental.payload.tableValue; }

  PARAM_REQUIRED_VARIABLES();

  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);

    NoiseType type = NoiseType(_type.payload.enumValue);
    switch (type) {
    case NoiseType::White:
      OVERRIDE_ACTIVATE(data, activateWhite);
      break;
    case NoiseType::Pink:
      OVERRIDE_ACTIVATE(data, activatePink);
      break;
    case NoiseType::Brown:
      OVERRIDE_ACTIVATE(data, activateBrown);
      break;
    }
    return CoreInfo::AudioType;
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);

    _deviceVar = referenceVariable(context, "Audio.Device");
    if (_deviceVar->valueType == SHType::Object) {
      _device = reinterpret_cast<Device *>(_deviceVar->payload.objectValue);
    }

    std::random_device rd;
    _rng.seed(rd());

    _pink_b0 = _pink_b1 = _pink_b2 = _pink_b3 = _pink_b4 = _pink_b5 = _pink_b6 = 0;
    _brown_last = 0;
  }

  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);

    if (_deviceVar) {
      releaseVariable(_deviceVar);
      _deviceVar = nullptr;
      _device = nullptr;
    }
  }

  SHVar activateWhite(SHContext *context, const SHVar &input) {
    const ma_uint32 channels = ma_uint32(_channels.payload.intValue);
    ma_uint64 nsamples = ma_uint64(_samples.payload.intValue);
    ma_uint32 sampleRate = ma_uint32(_sampleRate.payload.intValue);

    if (_device) {
      nsamples = getDeviceBufferSize(_device);
      sampleRate = getDeviceSampleRate(_device);
    }

    const float amp = float(_amplitude.get().payload.floatValue);
    _buffer.resize(channels * nsamples);

    // Generate white noise for each channel (sequential RNG calls, slight correlation)
    for (ma_uint32 ch = 0; ch < channels; ch++) {
      float *out = _buffer.data() + ch * nsamples;
      for (ma_uint64 i = 0; i < nsamples; i++) {
        out[i] = amp * _dist(_rng);
      }
    }

    return Var(makeAudio(_buffer.data(), uint32_t(nsamples), sampleRate, uint8_t(channels)));
  }

  SHVar activatePink(SHContext *context, const SHVar &input) {
    const ma_uint32 channels = ma_uint32(_channels.payload.intValue);
    ma_uint64 nsamples = ma_uint64(_samples.payload.intValue);
    ma_uint32 sampleRate = ma_uint32(_sampleRate.payload.intValue);

    if (_device) {
      nsamples = getDeviceBufferSize(_device);
      sampleRate = getDeviceSampleRate(_device);
    }

    const float amp = float(_amplitude.get().payload.floatValue);
    _buffer.resize(channels * nsamples);

    // Generate pink noise (same for all channels for coherent stereo)
    for (ma_uint64 i = 0; i < nsamples; i++) {
      float white = _dist(_rng);

      // Paul Kellet's refined method
      _pink_b0 = 0.99886f * _pink_b0 + white * 0.0555179f;
      _pink_b1 = 0.99332f * _pink_b1 + white * 0.0750759f;
      _pink_b2 = 0.96900f * _pink_b2 + white * 0.1538520f;
      _pink_b3 = 0.86650f * _pink_b3 + white * 0.3104856f;
      _pink_b4 = 0.55000f * _pink_b4 + white * 0.5329522f;
      _pink_b5 = -0.7616f * _pink_b5 - white * 0.0168980f;

      float pink = _pink_b0 + _pink_b1 + _pink_b2 + _pink_b3 + _pink_b4 + _pink_b5 + _pink_b6 + white * 0.5362f;
      _pink_b6 = white * 0.115926f;

      float sample = amp * pink * 0.11f;

      // Write to all channels
      for (ma_uint32 ch = 0; ch < channels; ch++) {
        _buffer[ch * nsamples + i] = sample;
      }
    }

    return Var(makeAudio(_buffer.data(), uint32_t(nsamples), sampleRate, uint8_t(channels)));
  }

  SHVar activateBrown(SHContext *context, const SHVar &input) {
    const ma_uint32 channels = ma_uint32(_channels.payload.intValue);
    ma_uint64 nsamples = ma_uint64(_samples.payload.intValue);
    ma_uint32 sampleRate = ma_uint32(_sampleRate.payload.intValue);

    if (_device) {
      nsamples = getDeviceBufferSize(_device);
      sampleRate = getDeviceSampleRate(_device);
    }

    const float amp = float(_amplitude.get().payload.floatValue);
    _buffer.resize(channels * nsamples);

    // Generate brown noise (same for all channels)
    for (ma_uint64 i = 0; i < nsamples; i++) {
      float white = _dist(_rng);

      _brown_last = (_brown_last + (0.02f * white)) / 1.02f;
      _brown_last = std::clamp(_brown_last, -1.0f, 1.0f);

      float sample = amp * _brown_last * 3.5f;

      // Write to all channels
      for (ma_uint32 ch = 0; ch < channels; ch++) {
        _buffer[ch * nsamples + i] = sample;
      }
    }

    return Var(makeAudio(_buffer.data(), uint32_t(nsamples), sampleRate, uint8_t(channels)));
  }

  SHVar activate(SHContext *context, const SHVar &input) { return activateWhite(context, input); }
};

} // namespace Synth
} // namespace Audio
} // namespace shards

SHARDS_REGISTER_FN(synth) {
  using namespace shards::Audio::Synth;
  REGISTER_ENUM(Oscillator::WaveformEnumInfo);
  REGISTER_ENUM(Noise::NoiseTypeEnumInfo);
  REGISTER_SHARD("Audio.Oscillator", Oscillator);
  REGISTER_SHARD("Audio.Noise", Noise);
}
