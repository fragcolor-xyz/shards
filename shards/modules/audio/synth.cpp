/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2024 Fragcolor Pte. Ltd. */

#include <shards/core/shared.hpp>
#include <shards/core/params.hpp>
#include <miniaudio.h>
#include <algorithm>
#include <array>
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
uint32_t getDeviceBufferSize(const Device *device);
uint32_t getDeviceSampleRate(const Device *device);

namespace Synth {

// Mathematical constants
static constexpr double TWO_PI = 2.0 * M_PI;
static constexpr float TWO_PI_F = float(TWO_PI);

// Maximum samples per buffer to prevent OOM (1 million samples ~= 22 seconds at 44.1kHz)
static constexpr size_t MAX_SAMPLES = 1000000;
// Maximum channels to prevent unreasonable allocations
static constexpr uint32_t MAX_CHANNELS = 32;

// Noise amplitude scaling factors (calibrated for approximately equal perceived loudness)
// Pink noise has higher RMS than white due to filter accumulation, scale down to prevent clipping
static constexpr float PINK_NOISE_SCALE = 0.11f;
// Brown noise needs boost to match white noise perceived loudness level
static constexpr float BROWN_NOISE_SCALE = 3.5f;

// =============================================================================
// SIMD-optimized transcendental functions for audio buffers
// NOTE: No INT_MAX guards needed. Audio buffers are validated at allocation time
// (MAX_SAMPLES = 1M) and device buffers are typically 256-8192 samples.
// =============================================================================

inline void simdSinf(float *out, const float *in, size_t count) {
#if defined(SYNTH_HAS_ACCELERATE)
  int n = static_cast<int>(count);
  vvsinf(out, in, &n);
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
  for (; i < count; ++i)
    out[i] = std::sin(in[i]);
#else
  for (size_t i = 0; i < count; ++i)
    out[i] = std::sin(in[i]);
#endif
}

inline void simdTanhf(float *out, const float *in, size_t count) {
#if defined(SYNTH_HAS_ACCELERATE)
  int n = static_cast<int>(count);
  vvtanhf(out, in, &n);
#elif defined(SYNTH_HAS_SLEEF)
  size_t i = 0;
#if defined(__AVX2__)
  for (; i + 8 <= count; i += 8) {
    __m256 va = _mm256_loadu_ps(in + i);
    __m256 vr = Sleef_tanhf8_u10avx2(va);
    _mm256_storeu_ps(out + i, vr);
  }
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
  for (; i + 4 <= count; i += 4) {
    float32x4_t va = vld1q_f32(in + i);
    float32x4_t vr = Sleef_tanhf4_u10advsimd(va);
    vst1q_f32(out + i, vr);
  }
#endif
  for (; i < count; ++i)
    out[i] = std::tanh(in[i]);
#else
  for (size_t i = 0; i < count; ++i)
    out[i] = std::tanh(in[i]);
#endif
}

// =============================================================================
// SIMD-optimized exp2 for exponential FM
// =============================================================================

inline void simdExp2f(float *out, const float *in, size_t count) {
#if defined(SYNTH_HAS_ACCELERATE)
  int n = static_cast<int>(count);
  vvexp2f(out, in, &n);
#elif defined(SYNTH_HAS_SLEEF)
  size_t i = 0;
#if defined(__AVX2__)
  for (; i + 8 <= count; i += 8) {
    __m256 va = _mm256_loadu_ps(in + i);
    __m256 vr = Sleef_exp2f8_u10avx2(va);
    _mm256_storeu_ps(out + i, vr);
  }
#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
  for (; i + 4 <= count; i += 4) {
    float32x4_t va = vld1q_f32(in + i);
    float32x4_t vr = Sleef_exp2f4_u10advsimd(va);
    vst1q_f32(out + i, vr);
  }
#endif
  for (; i < count; ++i)
    out[i] = std::exp2(in[i]);
#else
  for (size_t i = 0; i < count; ++i)
    out[i] = std::exp2(in[i]);
#endif
}

// =============================================================================
// Oscillator with FM support and multiple waveforms
// =============================================================================

struct Oscillator {
  enum class Waveform { Sine, Triangle, Sawtooth, Square };
  DECL_ENUM_INFO(Waveform, Waveform, "Type of waveform for the oscillator output.", 'wave');

  enum class FMMode { Linear, Exponential };
  DECL_ENUM_INFO(FMMode, FMMode, "FM synthesis mode: Linear (phase modulation) or Exponential (1V/oct style).", 'fmmd');

  // Parameters using PARAM macros
  PARAM_VAR(_waveform, "Waveform", "The waveform type (Sine, Triangle, Sawtooth, Square).", {WaveformEnumInfo::Type});
  PARAM_PARAMVAR(_frequency, "Frequency", "The base frequency in Hz.", {CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_amplitude, "Amplitude", "Output amplitude (0.0 to 1.0).", {CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_index, "Index", "Modulation index - controls FM depth when audio input is provided.",
                 {CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_VAR(_fmMode, "FMMode", "FM synthesis mode: Linear (phase modulation, DX7-style) or Exponential (1V/oct, modular-style).",
            {FMModeEnumInfo::Type});
  PARAM_VAR(_channels, "Channels", "Number of output channels.", {CoreInfo::IntType});
  PARAM_VAR(_sampleRate, "SampleRate", "Sample rate in Hz. Ignored if inside Audio.Channel.", {CoreInfo::IntType});
  PARAM_VAR(_samples, "Samples", "Number of samples per buffer. Ignored if inside Audio.Channel.", {CoreInfo::IntType});

  PARAM_IMPL(PARAM_IMPL_FOR(_waveform), PARAM_IMPL_FOR(_frequency), PARAM_IMPL_FOR(_amplitude), PARAM_IMPL_FOR(_index),
             PARAM_IMPL_FOR(_fmMode), PARAM_IMPL_FOR(_channels), PARAM_IMPL_FOR(_sampleRate), PARAM_IMPL_FOR(_samples));

  // Internal state
  double _phase{0.0};                                // For carrier mode (all channels share)
  std::array<double, MAX_CHANNELS> _channelPhases{}; // For modulated modes (per-channel)
  std::vector<float> _buffer;
  std::vector<float> _phaseBuffer;
  std::vector<float> _modPhaseBuffer;
  std::vector<float> _freqScaleBuffer; // For exponential FM: stores 2^(modIndex * modulator)

  SHVar *_deviceVar{nullptr};
  Device *_device{nullptr};

  Oscillator() {
    _waveform = Var::Enum(Waveform::Sine, CoreCC, WaveformEnumInfo::TypeId);
    _frequency = Var(440.0);
    _amplitude = Var(1.0);
    _index = Var(1.0);
    _fmMode = Var::Enum(FMMode::Linear, CoreCC, FMModeEnumInfo::TypeId);
    _channels = Var(1);
    _sampleRate = Var(44100);
    _samples = Var(1024);
  }

  static SHOptionalString help() {
    return SHCCSTR("Audio oscillator with multiple waveforms and FM synthesis support. "
                   "When input is None, generates a waveform at the specified frequency. "
                   "When input is Audio, uses it for FM synthesis. Linear mode (default) adds modulator to phase (DX7-style). "
                   "Exponential mode scales frequency by 2^(Index*modulator) for 1V/oct modular-style FM. "
                   "Multiple oscillators can be chained for complex FM patches.");
  }

  static inline Types InputTypes{{CoreInfo::NoneType, CoreInfo::AudioType}};
  static SHTypesInfo inputTypes() { return InputTypes; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("None to generate a carrier wave, or Audio to use as phase modulation signal.");
  }
  static SHTypesInfo outputTypes() { return CoreInfo::AudioType; }
  static SHOptionalString outputHelp() { return SHCCSTR("The generated or modulated audio signal."); }


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
      const FMMode fmMode = FMMode(_fmMode.payload.enumValue);
      const bool isLinear = (fmMode == FMMode::Linear);

// Dispatch macro for Waveform × FMMode combinations
#define DISPATCH_MODULATED(W)                                                                                                      \
  if (isLinear) {                                                                                                                  \
    OVERRIDE_ACTIVATE(data, activateModulatedLinear<W>);                                                                           \
  } else {                                                                                                                         \
    OVERRIDE_ACTIVATE(data, activateModulatedExp<W>);                                                                              \
  }

      switch (waveform) {
      case Waveform::Sine:
        DISPATCH_MODULATED(Waveform::Sine);
        break;
      case Waveform::Triangle:
        DISPATCH_MODULATED(Waveform::Triangle);
        break;
      case Waveform::Sawtooth:
        DISPATCH_MODULATED(Waveform::Sawtooth);
        break;
      case Waveform::Square:
        DISPATCH_MODULATED(Waveform::Square);
        break;
      }
#undef DISPATCH_MODULATED
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
    _channelPhases.fill(0.0);
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
  // NOTE: Triangle, Sawtooth, and Square use naive (non-bandlimited) generation.
  // This may produce aliasing artifacts at higher frequencies (especially above sampleRate/4).
  // For most synthesis applications this is acceptable; consider BLIT/BLEP for anti-aliased waveforms.
  template <Waveform W> inline float generateSample(float phase) {
    // Normalize phase to 0-1 range
    float t = phase / TWO_PI_F;
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

    // Validate buffer size to prevent OOM
    if (nsamples > MAX_SAMPLES || channels > MAX_CHANNELS) {
      throw ActivationError("Audio.Oscillator: buffer size exceeds limits");
    }

    const double freq = _frequency.get().payload.floatValue;
    const float amp = float(_amplitude.get().payload.floatValue);

    _buffer.resize(channels * nsamples);
    _phaseBuffer.resize(nsamples);

    const double phaseInc = (TWO_PI * freq) / double(sampleRate);

    // Build phase array
    double phase = _phase;
    for (ma_uint64 i = 0; i < nsamples; i++) {
      _phaseBuffer[i] = float(phase);
      phase += phaseInc;
    }
    // PHASE WRAPPING: Use fmod, not a while loop.
    // At high frequencies (e.g., 20kHz @ 44.1kHz), phaseInc ≈ 2.85 rad/sample.
    // After 1024 samples, phase ≈ 2920 radians. A while loop would need ~465 iterations.
    // fmod is O(1) and handles any frequency. The "precision loss" concern is negligible
    // for double-precision - fmod preserves mantissa bits just as well as subtraction.
    _phase = std::fmod(phase, TWO_PI);

    // Generate waveform (output to first channel position)
    generateWaveform<W>(_buffer.data(), _phaseBuffer.data(), nsamples);

    // Apply amplitude
    for (ma_uint64 i = 0; i < nsamples; i++) {
      _buffer[i] *= amp;
    }

    // Copy to other channels if needed (planar format) - skip for mono
    if (channels > 1) {
      for (ma_uint32 ch = 1; ch < channels; ch++) {
        std::memcpy(_buffer.data() + ch * nsamples, _buffer.data(), nsamples * sizeof(float));
      }
    }

    return Var(makeAudio(_buffer.data(), uint32_t(nsamples), sampleRate, uint8_t(channels)));
  }

  // Linear FM (phase modulation) from input audio - templated for compile-time waveform dispatch
  // This is the classic DX7-style FM where modulator is added directly to phase
  template <Waveform W> SHVar activateModulatedLinear(SHContext *context, const SHVar &input) {
    const auto &audio = input.payload.audioValue;
    const uint32_t nsamples = audio.nsamples;
    const uint32_t sampleRate = audioGetSampleRate(audio);
    const double baseFreq = _frequency.get().payload.floatValue;
    const float amp = float(_amplitude.get().payload.floatValue);
    const float modIndex = float(_index.get().payload.floatValue);

    const uint32_t channels = audio.channels;

    // Validate buffer size to prevent OOM
    if (nsamples > MAX_SAMPLES || channels > MAX_CHANNELS) {
      throw ActivationError("Audio.Oscillator: buffer size exceeds limits");
    }

    _buffer.resize(channels * nsamples);
    _phaseBuffer.resize(nsamples);
    _modPhaseBuffer.resize(nsamples);

    const double phaseInc = (TWO_PI * baseFreq) / double(sampleRate);

    // Build base phase array once (shared across all channels)
    double phase = _phase;
    for (uint32_t i = 0; i < nsamples; i++) {
      _phaseBuffer[i] = float(phase);
      phase += phaseInc;
    }
    // See comment in activateCarrier for why fmod is used here
    _phase = std::fmod(phase, TWO_PI);

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

  // Exponential FM from input audio - templated for compile-time waveform dispatch
  // This is modular/analog-style FM where modulator scales frequency exponentially (1V/oct style)
  // freq(t) = baseFreq * 2^(modIndex * modulator(t))
  template <Waveform W> SHVar activateModulatedExp(SHContext *context, const SHVar &input) {
    const auto &audio = input.payload.audioValue;
    const uint32_t nsamples = audio.nsamples;
    const uint32_t sampleRate = audioGetSampleRate(audio);
    const double baseFreq = _frequency.get().payload.floatValue;
    const float amp = float(_amplitude.get().payload.floatValue);
    const float modIndex = float(_index.get().payload.floatValue);

    const uint32_t channels = audio.channels;

    // Validate buffer size to prevent OOM
    if (nsamples > MAX_SAMPLES || channels > MAX_CHANNELS) {
      throw ActivationError("Audio.Oscillator: buffer size exceeds limits");
    }

    _buffer.resize(channels * nsamples);
    _modPhaseBuffer.resize(nsamples);
    _freqScaleBuffer.resize(nsamples);

    const double invSampleRate = 1.0 / double(sampleRate);

    // Process each channel independently with per-channel phase tracking
    // This matches linear FM behavior where each channel is modulated independently
    for (uint32_t ch = 0; ch < channels; ch++) {
      const float *modSamples = audio.samples + ch * nsamples;
      float *outSamples = _buffer.data() + ch * nsamples;

      // Compute frequency scale factors: 2^(modIndex * modulator)
      // Clamp exponent to ±10 octaves to prevent extreme frequencies and numerical instability
      for (uint32_t i = 0; i < nsamples; i++) {
        _freqScaleBuffer[i] = std::clamp(modIndex * modSamples[i], -10.0f, 10.0f);
      }
      simdExp2f(_freqScaleBuffer.data(), _freqScaleBuffer.data(), nsamples);

      // Accumulate phase with variable frequency using per-channel phase state
      double phase = _channelPhases[ch];
      for (uint32_t i = 0; i < nsamples; i++) {
        _modPhaseBuffer[i] = float(phase);
        double freq = baseFreq * double(_freqScaleBuffer[i]);
        phase += TWO_PI * freq * invSampleRate;
      }
      _channelPhases[ch] = std::fmod(phase, TWO_PI);

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
                   "and brown/Brownian noise (1/f^2 spectrum). "
                   "Note: White noise generates independent samples per channel (decorrelated stereo), "
                   "while pink and brown noise use the same signal for all channels (coherent stereo).");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString inputHelp() { return DefaultHelpText::InputHelpIgnored; }
  static SHTypesInfo outputTypes() { return CoreInfo::AudioType; }
  static SHOptionalString outputHelp() { return SHCCSTR("The generated noise signal."); }


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

    // Validate buffer size to prevent OOM
    if (nsamples > MAX_SAMPLES || channels > MAX_CHANNELS) {
      throw ActivationError("Audio.Noise: buffer size exceeds limits");
    }

    const float amp = float(_amplitude.get().payload.floatValue);
    _buffer.resize(channels * nsamples);

    // Generate white noise - planar writes for cache locality
    // Each channel gets independent samples (decorrelated stereo)
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

    // Validate buffer size to prevent OOM
    if (nsamples > MAX_SAMPLES || channels > MAX_CHANNELS) {
      throw ActivationError("Audio.Noise: buffer size exceeds limits");
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

      float sample = amp * pink * PINK_NOISE_SCALE;

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

    // Validate buffer size to prevent OOM
    if (nsamples > MAX_SAMPLES || channels > MAX_CHANNELS) {
      throw ActivationError("Audio.Noise: buffer size exceeds limits");
    }

    const float amp = float(_amplitude.get().payload.floatValue);
    _buffer.resize(channels * nsamples);

    // Generate brown noise (same for all channels)
    for (ma_uint64 i = 0; i < nsamples; i++) {
      float white = _dist(_rng);

      // Leaky integrator with tanh soft limiting - prevents audible clicks from hard clipping
      // while keeping the signal bounded. tanh is smooth and fast (SIMD-optimized on most platforms).
      _brown_last = std::tanh((_brown_last + (0.02f * white)) / 1.02f);

      float sample = amp * _brown_last * BROWN_NOISE_SCALE;

      // Write to all channels
      for (ma_uint32 ch = 0; ch < channels; ch++) {
        _buffer[ch * nsamples + i] = sample;
      }
    }

    return Var(makeAudio(_buffer.data(), uint32_t(nsamples), sampleRate, uint8_t(channels)));
  }

  SHVar activate(SHContext *context, const SHVar &input) { return activateWhite(context, input); }
};

// =============================================================================
// ADSR Envelope Generator
// =============================================================================

struct Envelope {
  enum class Stage { Idle, Attack, Decay, Sustain, Release };

  enum class Curve { Linear, Exponential };
  DECL_ENUM_INFO(Curve, EnvelopeCurve, "Envelope curve type.", 'ecur');

  // Parameters
  PARAM_PARAMVAR(_attack, "Attack", "Attack time in seconds (0.001 to 10.0).", {CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_decay, "Decay", "Decay time in seconds (0.001 to 10.0).", {CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_sustain, "Sustain", "Sustain level (0.0 to 1.0).", {CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_release, "Release", "Release time in seconds (0.001 to 10.0).", {CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_VAR(_curve, "Curve", "Envelope curve type (Linear or Exponential).", {EnvelopeCurveEnumInfo::Type});

  PARAM_IMPL(PARAM_IMPL_FOR(_attack), PARAM_IMPL_FOR(_decay), PARAM_IMPL_FOR(_sustain), PARAM_IMPL_FOR(_release),
             PARAM_IMPL_FOR(_curve));

  // Per-channel state
  struct ChannelState {
    Stage stage{Stage::Idle};
    double level{0.0};
    double releaseStartLevel{0.0};
    uint64_t stageSamples{0};
  };
  std::array<ChannelState, MAX_CHANNELS> _channelStates{};

  std::vector<float> _buffer;

  SHVar *_deviceVar{nullptr};
  Device *_device{nullptr};

  Envelope() {
    _attack = Var(0.01);  // 10ms default attack
    _decay = Var(0.1);    // 100ms default decay
    _sustain = Var(0.7);  // 70% sustain level
    _release = Var(0.3);  // 300ms release
    _curve = Var::Enum(Curve::Exponential, CoreCC, EnvelopeCurveEnumInfo::TypeId);
  }

  static SHOptionalString help() {
    return SHCCSTR("ADSR envelope generator. Takes a gate signal (audio or trigger) as input, "
                   "where values > 0 indicate gate on and values <= 0 indicate gate off. "
                   "Outputs an envelope signal from 0.0 to 1.0 that can be multiplied with "
                   "an oscillator for amplitude modulation (VCA).");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::AudioType; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("Gate signal where > 0 is gate on, <= 0 is gate off.");
  }
  static SHTypesInfo outputTypes() { return CoreInfo::AudioType; }
  static SHOptionalString outputHelp() { return SHCCSTR("Envelope values from 0.0 to 1.0."); }

  PARAM_REQUIRED_VARIABLES();

  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);

    Curve curve = Curve(_curve.payload.enumValue);
    if (curve == Curve::Linear) {
      OVERRIDE_ACTIVATE(data, activateLinear);
    } else {
      OVERRIDE_ACTIVATE(data, activateExponential);
    }
    return CoreInfo::AudioType;
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);

    _deviceVar = referenceVariable(context, "Audio.Device");
    if (_deviceVar->valueType == SHType::Object) {
      _device = reinterpret_cast<Device *>(_deviceVar->payload.objectValue);
    }

    // Reset all channel states
    for (auto &state : _channelStates) {
      state = ChannelState{};
    }
  }

  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);

    if (_deviceVar) {
      releaseVariable(_deviceVar);
      _deviceVar = nullptr;
      _device = nullptr;
    }
  }

  template <typename ProcessFunc> SHVar processEnvelope(const SHVar &input, ProcessFunc processFunc) {
    const auto &audio = input.payload.audioValue;
    const uint32_t nsamples = audio.nsamples;
    const uint32_t sampleRate = audioGetSampleRate(audio);
    const uint32_t channels = audio.channels;

    // Get parameters
    const double attack = std::max(0.001, _attack.get().payload.floatValue);
    const double decay = std::max(0.001, _decay.get().payload.floatValue);
    const double sustain = std::clamp(_sustain.get().payload.floatValue, 0.0, 1.0);
    const double release = std::max(0.001, _release.get().payload.floatValue);

    // Calculate samples for each stage
    const uint64_t attackSamples = uint64_t(attack * sampleRate);
    const uint64_t decaySamples = uint64_t(decay * sampleRate);
    const uint64_t releaseSamples = uint64_t(release * sampleRate);

    // Validate buffer size
    if (nsamples > MAX_SAMPLES || channels > MAX_CHANNELS) {
      throw ActivationError("Audio.Envelope: buffer size exceeds limits");
    }

    _buffer.resize(channels * nsamples);

    // Process each channel independently
    for (uint32_t ch = 0; ch < channels; ch++) {
      const float *gateSamples = audio.samples + ch * nsamples;
      float *outSamples = _buffer.data() + ch * nsamples;
      ChannelState &state = _channelStates[ch];

      for (uint32_t i = 0; i < nsamples; i++) {
        bool gateOn = gateSamples[i] > 0.0f;

        // Handle gate transitions
        if (gateOn && (state.stage == Stage::Idle || state.stage == Stage::Release)) {
          // Gate on - start attack (retrigger support)
          state.stage = Stage::Attack;
          state.stageSamples = 0;
          // level continues from current value for smooth retrigger
        } else if (!gateOn && state.stage != Stage::Idle && state.stage != Stage::Release) {
          // Gate off - start release
          state.stage = Stage::Release;
          state.releaseStartLevel = state.level;
          state.stageSamples = 0;
        }

        // Process current stage
        processFunc(state, sustain, attackSamples, decaySamples, releaseSamples);

        outSamples[i] = float(state.level);
        state.stageSamples++;
      }
    }

    return Var(makeAudio(_buffer.data(), nsamples, sampleRate, uint8_t(channels)));
  }

  SHVar activateLinear(SHContext *context, const SHVar &input) {
    return processEnvelope(input, [](ChannelState &state, double sustain, uint64_t attackSamples, uint64_t decaySamples,
                                     uint64_t releaseSamples) {
      switch (state.stage) {
      case Stage::Attack: {
        double t = double(state.stageSamples) / double(attackSamples);
        state.level = t;
        if (state.stageSamples >= attackSamples) {
          state.stage = Stage::Decay;
          state.stageSamples = 0;
          state.level = 1.0;
        }
        break;
      }
      case Stage::Decay: {
        double t = double(state.stageSamples) / double(decaySamples);
        state.level = 1.0 - t * (1.0 - sustain);
        if (state.stageSamples >= decaySamples) {
          state.stage = Stage::Sustain;
          state.stageSamples = 0;
          state.level = sustain;
        }
        break;
      }
      case Stage::Sustain:
        state.level = sustain;
        break;
      case Stage::Release: {
        double t = double(state.stageSamples) / double(releaseSamples);
        state.level = state.releaseStartLevel * (1.0 - t);
        if (state.stageSamples >= releaseSamples) {
          state.stage = Stage::Idle;
          state.stageSamples = 0;
          state.level = 0.0;
        }
        break;
      }
      case Stage::Idle:
        state.level = 0.0;
        break;
      }
    });
  }

  SHVar activateExponential(SHContext *context, const SHVar &input) {
    return processEnvelope(input, [](ChannelState &state, double sustain, uint64_t attackSamples, uint64_t decaySamples,
                                     uint64_t releaseSamples) {
      // Exponential curve coefficient (higher = faster curve, 5.0 gives natural envelope feel)
      constexpr double EXP_COEFF = 5.0;

      switch (state.stage) {
      case Stage::Attack: {
        double t = double(state.stageSamples) / double(attackSamples);
        // Exponential attack: 1 - e^(-kt)
        state.level = 1.0 - std::exp(-EXP_COEFF * t);
        if (state.stageSamples >= attackSamples) {
          state.stage = Stage::Decay;
          state.stageSamples = 0;
          state.level = 1.0;
        }
        break;
      }
      case Stage::Decay: {
        double t = double(state.stageSamples) / double(decaySamples);
        // Exponential decay from 1.0 to sustain
        state.level = sustain + (1.0 - sustain) * std::exp(-EXP_COEFF * t);
        if (state.stageSamples >= decaySamples) {
          state.stage = Stage::Sustain;
          state.stageSamples = 0;
          state.level = sustain;
        }
        break;
      }
      case Stage::Sustain:
        state.level = sustain;
        break;
      case Stage::Release: {
        double t = double(state.stageSamples) / double(releaseSamples);
        // Exponential release from releaseStartLevel to 0
        state.level = state.releaseStartLevel * std::exp(-EXP_COEFF * t);
        if (state.stageSamples >= releaseSamples) {
          state.stage = Stage::Idle;
          state.stageSamples = 0;
          state.level = 0.0;
        }
        break;
      }
      case Stage::Idle:
        state.level = 0.0;
        break;
      }
    });
  }

  SHVar activate(SHContext *context, const SHVar &input) { return activateExponential(context, input); }
};

// =============================================================================
// State Variable Filter (SVF)
// =============================================================================

struct Filter {
  enum class FilterType { Lowpass, Highpass, Bandpass, Notch };
  DECL_ENUM_INFO(FilterType, FilterType, "Type of filter response.", 'filt');

  // Parameters
  PARAM_VAR(_type, "Type", "Filter type (Lowpass, Highpass, Bandpass, Notch).", {FilterTypeEnumInfo::Type});
  PARAM_PARAMVAR(_cutoff, "Cutoff", "Cutoff frequency in Hz.", {CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_resonance, "Resonance", "Resonance amount (0.0 to 1.0).", {CoreInfo::FloatType, CoreInfo::FloatVarType});

  PARAM_IMPL(PARAM_IMPL_FOR(_type), PARAM_IMPL_FOR(_cutoff), PARAM_IMPL_FOR(_resonance));

  // Per-channel SVF state (Cytomic/Vadim Zavalishin style)
  struct SVFState {
    double ic1eq{0.0}; // Integrator 1 state
    double ic2eq{0.0}; // Integrator 2 state
  };
  std::array<SVFState, MAX_CHANNELS> _svfStates{};

  std::vector<float> _buffer;

  SHVar *_deviceVar{nullptr};
  Device *_device{nullptr};

  Filter() {
    _type = Var::Enum(FilterType::Lowpass, CoreCC, FilterTypeEnumInfo::TypeId);
    _cutoff = Var(1000.0);   // 1kHz default cutoff
    _resonance = Var(0.5);   // 50% resonance
  }

  static SHOptionalString help() {
    return SHCCSTR("State Variable Filter (SVF) with Lowpass, Highpass, Bandpass, and Notch modes. "
                   "Cutoff and Resonance can be modulated for filter sweeps and effects.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::AudioType; }
  static SHOptionalString inputHelp() { return SHCCSTR("Audio signal to filter."); }
  static SHTypesInfo outputTypes() { return CoreInfo::AudioType; }
  static SHOptionalString outputHelp() { return SHCCSTR("Filtered audio signal."); }

  PARAM_REQUIRED_VARIABLES();

  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);

    FilterType type = FilterType(_type.payload.enumValue);
    switch (type) {
    case FilterType::Lowpass:
      OVERRIDE_ACTIVATE(data, activateLowpass);
      break;
    case FilterType::Highpass:
      OVERRIDE_ACTIVATE(data, activateHighpass);
      break;
    case FilterType::Bandpass:
      OVERRIDE_ACTIVATE(data, activateBandpass);
      break;
    case FilterType::Notch:
      OVERRIDE_ACTIVATE(data, activateNotch);
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

    // Reset filter states
    for (auto &state : _svfStates) {
      state = SVFState{};
    }
  }

  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);

    if (_deviceVar) {
      releaseVariable(_deviceVar);
      _deviceVar = nullptr;
      _device = nullptr;
    }
  }

  // SVF core processing (Cytomic/Vadim Zavalishin TPT implementation)
  // Returns: lowpass, highpass, bandpass outputs via reference
  inline void processSVF(double v0, double g, double k, SVFState &state, double &lowpass, double &highpass, double &bandpass) {
    // TPT SVF equations
    double v1 = (state.ic1eq + g * (v0 - state.ic2eq)) / (1.0 + g * (g + k));
    double v2 = state.ic2eq + g * v1;

    // Update state
    state.ic1eq = 2.0 * v1 - state.ic1eq;
    state.ic2eq = 2.0 * v2 - state.ic2eq;

    // Outputs
    lowpass = v2;
    bandpass = v1;
    highpass = v0 - k * v1 - v2;
  }

  template <FilterType Type> SHVar processFilter(SHContext *context, const SHVar &input) {
    const auto &audio = input.payload.audioValue;
    const uint32_t nsamples = audio.nsamples;
    const uint32_t sampleRate = audioGetSampleRate(audio);
    const uint32_t channels = audio.channels;

    // Get parameters
    const double cutoff = std::clamp(_cutoff.get().payload.floatValue, 20.0, double(sampleRate) * 0.49);
    const double resonance = std::clamp(_resonance.get().payload.floatValue, 0.0, 1.0);

    // SVF coefficients
    // g = tan(π * cutoff / sampleRate)
    const double g = std::tan(M_PI * cutoff / double(sampleRate));
    // k = 2.0 - 2.0 * resonance (resonance 0 -> k=2, resonance 1 -> k=0)
    // k=0 gives self-oscillation, we clamp to avoid instability
    const double k = 2.0 - 2.0 * resonance * 0.99;

    // Validate buffer size
    if (nsamples > MAX_SAMPLES || channels > MAX_CHANNELS) {
      throw ActivationError("Audio.Filter: buffer size exceeds limits");
    }

    _buffer.resize(channels * nsamples);

    // Process each channel independently
    for (uint32_t ch = 0; ch < channels; ch++) {
      const float *inSamples = audio.samples + ch * nsamples;
      float *outSamples = _buffer.data() + ch * nsamples;
      SVFState &state = _svfStates[ch];

      for (uint32_t i = 0; i < nsamples; i++) {
        double v0 = double(inSamples[i]);
        double lowpass, highpass, bandpass;
        processSVF(v0, g, k, state, lowpass, highpass, bandpass);

        // Select output based on filter type (compile-time dispatch)
        if constexpr (Type == FilterType::Lowpass) {
          outSamples[i] = float(lowpass);
        } else if constexpr (Type == FilterType::Highpass) {
          outSamples[i] = float(highpass);
        } else if constexpr (Type == FilterType::Bandpass) {
          outSamples[i] = float(bandpass);
        } else if constexpr (Type == FilterType::Notch) {
          // Notch = lowpass + highpass
          outSamples[i] = float(lowpass + highpass);
        }
      }
    }

    return Var(makeAudio(_buffer.data(), nsamples, sampleRate, uint8_t(channels)));
  }

  SHVar activateLowpass(SHContext *context, const SHVar &input) { return processFilter<FilterType::Lowpass>(context, input); }
  SHVar activateHighpass(SHContext *context, const SHVar &input) { return processFilter<FilterType::Highpass>(context, input); }
  SHVar activateBandpass(SHContext *context, const SHVar &input) { return processFilter<FilterType::Bandpass>(context, input); }
  SHVar activateNotch(SHContext *context, const SHVar &input) { return processFilter<FilterType::Notch>(context, input); }

  SHVar activate(SHContext *context, const SHVar &input) { return activateLowpass(context, input); }
};

} // namespace Synth
} // namespace Audio
} // namespace shards

SHARDS_REGISTER_FN(synth) {
  using namespace shards::Audio::Synth;
  REGISTER_ENUM(Oscillator::WaveformEnumInfo);
  REGISTER_ENUM(Oscillator::FMModeEnumInfo);
  REGISTER_ENUM(Noise::NoiseTypeEnumInfo);
  REGISTER_ENUM(Envelope::EnvelopeCurveEnumInfo);
  REGISTER_ENUM(Filter::FilterTypeEnumInfo);
  REGISTER_SHARD("Audio.Oscillator", Oscillator);
  REGISTER_SHARD("Audio.Noise", Noise);
  REGISTER_SHARD("Audio.Envelope", Envelope);
  REGISTER_SHARD("Audio.Filter", Filter);
}
