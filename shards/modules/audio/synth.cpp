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

// SIMD headers - needed for both Apple and non-Apple (multi-channel filter processing)
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>
#define SYNTH_HAS_NEON 1
#elif defined(__AVX2__) || defined(__SSE__)
#include <immintrin.h>
#define SYNTH_HAS_SSE 1
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
  PARAM_PARAMVAR(_retrigger, "Retrigger", "If true, retrigger envelope on gate even during Decay/Sustain stages. If false (default), "
                                          "only retrigger from Idle/Release (legato behavior).", {CoreInfo::BoolType, CoreInfo::BoolVarType});

  PARAM_IMPL(PARAM_IMPL_FOR(_attack), PARAM_IMPL_FOR(_decay), PARAM_IMPL_FOR(_sustain), PARAM_IMPL_FOR(_release),
             PARAM_IMPL_FOR(_curve), PARAM_IMPL_FOR(_retrigger));

  // Per-channel state
  struct ChannelState {
    Stage stage{Stage::Idle};
    double level{0.0};
    double attackStartLevel{0.0};  // For smooth retrigger - where attack starts from
    double releaseStartLevel{0.0};
    uint64_t stageSamples{0};
  };
  std::array<ChannelState, MAX_CHANNELS> _channelStates{};

  std::vector<float> _buffer;

  Envelope() {
    _attack = Var(0.01);  // 10ms default attack
    _decay = Var(0.1);    // 100ms default decay
    _sustain = Var(0.7);  // 70% sustain level
    _release = Var(0.3);  // 300ms release
    _curve = Var::Enum(Curve::Exponential, CoreCC, EnvelopeCurveEnumInfo::TypeId);
    _retrigger = Var(false);  // Default to legato behavior
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

    // Reset all channel states
    for (auto &state : _channelStates) {
      state = ChannelState{};
    }
  }

  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);
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
    const bool retrigger = _retrigger.get().payload.boolValue;

    // Calculate samples for each stage (round to nearest, minimum 1 sample to avoid div-by-zero)
    const uint64_t attackSamples = std::max(uint64_t(1), uint64_t(std::round(attack * sampleRate)));
    const uint64_t decaySamples = std::max(uint64_t(1), uint64_t(std::round(decay * sampleRate)));
    const uint64_t releaseSamples = std::max(uint64_t(1), uint64_t(std::round(release * sampleRate)));

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
        // Default (legato): only retrigger from Idle/Release
        // With Retrigger=true: also retrigger from Decay/Sustain (polyphonic style)
        bool canRetrigger = (state.stage == Stage::Idle || state.stage == Stage::Release) ||
                            (retrigger && (state.stage == Stage::Decay || state.stage == Stage::Sustain));
        if (gateOn && canRetrigger) {
          // Gate on - start attack from current level (smooth retrigger)
          state.stage = Stage::Attack;
          state.attackStartLevel = state.level;  // Remember where we're starting from
          state.stageSamples = 0;
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
        // Linear interpolation from attackStartLevel to 1.0 (smooth retrigger)
        state.level = state.attackStartLevel + t * (1.0 - state.attackStartLevel);
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
      // Exponential curve coefficient: 5.0 means the curve reaches ~99.3% (1 - e^-5) of its
      // target at t=1.0. This gives a natural-sounding envelope with most movement in the
      // early portion of each stage, mimicking capacitor charge/discharge behavior.
      constexpr double EXP_COEFF = 5.0;

      switch (state.stage) {
      case Stage::Attack: {
        double t = double(state.stageSamples) / double(attackSamples);
        // Exponential attack from attackStartLevel to 1.0 (smooth retrigger)
        // Uses asymptotic approach: level = start + (target - start) * (1 - e^(-kt))
        double attackRange = 1.0 - state.attackStartLevel;
        state.level = state.attackStartLevel + attackRange * (1.0 - std::exp(-EXP_COEFF * t));
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
// Multi-Stage Envelope Generator - Unlimited stages with per-stage curves
// =============================================================================

struct MultiStageEnvelope {
  // Stage definition: [level, time, curve]
  // - level: target level (0.0 to 1.0)
  // - time: time to reach this level in seconds
  // - curve: -1.0 (log) to 0.0 (linear) to 1.0 (exp)
  static inline Types StageType{{CoreInfo::Float3Type}};
  static inline Type StagesSeqType = Type::SeqOf(CoreInfo::Float3Type);
  static inline Types StagesTypes{{StagesSeqType}};

  // Parameters
  PARAM_PARAMVAR(_stages, "Stages", "Sequence of [level, time, curve] stages. Each stage defines target level, "
                                    "time to reach it, and curve shape (-1=log, 0=linear, 1=exp).",
                 {StagesSeqType});
  PARAM_PARAMVAR(_sustainIndex, "SustainIndex",
                 "Index of stage to sustain at until gate off (-1 for one-shot, no sustain).",
                 {CoreInfo::IntType, CoreInfo::IntVarType});
  PARAM_PARAMVAR(_release, "Release", "Sequence of [level, time, curve] stages for release phase after gate off.",
                 {StagesSeqType});
  PARAM_PARAMVAR(_loop, "Loop", "If true, loop through stages (excluding release) for LFO-like behavior.",
                 {CoreInfo::BoolType, CoreInfo::BoolVarType});
  PARAM_PARAMVAR(_loopStart, "LoopStart", "Stage index to loop back to (default 0).",
                 {CoreInfo::IntType, CoreInfo::IntVarType});

  PARAM_IMPL(PARAM_IMPL_FOR(_stages), PARAM_IMPL_FOR(_sustainIndex), PARAM_IMPL_FOR(_release), PARAM_IMPL_FOR(_loop),
             PARAM_IMPL_FOR(_loopStart));

  // Per-channel state
  struct ChannelState {
    enum class Phase { Idle, Attack, Sustain, Release };

    Phase phase{Phase::Idle};
    uint32_t currentStage{0};
    double level{0.0};
    double startLevel{0.0};  // Level at start of current stage
    uint64_t stageSamples{0};
    uint64_t totalStageSamples{0};  // Total samples for current stage
    bool prevGate{false};  // Previous gate state for edge detection
  };

  std::array<ChannelState, MAX_CHANNELS> _channelStates{};
  std::vector<float> _buffer;

  // Cached stage data for faster access
  struct StageData {
    float level;
    float time;
    float curve;
  };
  std::vector<StageData> _stagesCache;
  std::vector<StageData> _releaseCache;
  int32_t _sustainIdx{-1};
  bool _looping{false};
  uint32_t _loopStartIdx{0};

  MultiStageEnvelope() {
    // Default: simple ADSR-like shape
    // Stages must be provided by user - we start with empty sequences
    // Example in shards script:
    //   Audio.MultiStageEnvelope(
    //     Stages: [@f3(0.0 0.0 0.0) @f3(1.0 0.01 0.5) @f3(0.7 0.1 -0.3)]
    //     SustainIndex: 2
    //     Release: [@f3(0.0 0.3 -0.5)]
    //   )
    _sustainIndex = Var(-1);  // Default: no sustain (one-shot)
    _loop = Var(false);
    _loopStart = Var(0);
  }

  static SHOptionalString help() {
    return SHCCSTR("Multi-stage envelope generator with unlimited stages and per-stage curve control. "
                   "Each stage is defined as [level, time, curve] where curve ranges from -1 (logarithmic) "
                   "through 0 (linear) to 1 (exponential). Supports sustain at any stage, looping for "
                   "LFO-like behavior, and separate release stages. Retriggering from any phase restarts "
                   "attack from current level (no discontinuities). Parameters can be changed dynamically.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::AudioType; }
  static SHOptionalString inputHelp() { return SHCCSTR("Gate signal where > 0 is gate on, <= 0 is gate off."); }
  static SHTypesInfo outputTypes() { return CoreInfo::AudioType; }
  static SHOptionalString outputHelp() { return SHCCSTR("Envelope values from 0.0 to 1.0."); }

  PARAM_REQUIRED_VARIABLES();

  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return CoreInfo::AudioType;
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);

    // Reset channel states
    for (auto &state : _channelStates) {
      state = ChannelState{};
    }

    // Cache stage data
    cacheStages();
  }

  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  void cacheStages() {
    _stagesCache.clear();
    _releaseCache.clear();

    // Cache main stages
    const auto &stagesVar = _stages.get();
    if (stagesVar.valueType == SHType::Seq) {
      auto &stagesSeq = stagesVar.payload.seqValue;
      for (uint32_t i = 0; i < stagesSeq.len; i++) {
        auto &stage = stagesSeq.elements[i];
        _stagesCache.push_back({stage.payload.float3Value[0], stage.payload.float3Value[1], stage.payload.float3Value[2]});
      }
    }

    // Cache release stages
    const auto &releaseVar = _release.get();
    if (releaseVar.valueType == SHType::Seq) {
      auto &releaseSeq = releaseVar.payload.seqValue;
      for (uint32_t i = 0; i < releaseSeq.len; i++) {
        auto &stage = releaseSeq.elements[i];
        _releaseCache.push_back({stage.payload.float3Value[0], stage.payload.float3Value[1], stage.payload.float3Value[2]});
      }
    }

    _sustainIdx = int32_t(_sustainIndex.get().payload.intValue);
    _looping = _loop.get().payload.boolValue;
    _loopStartIdx = uint32_t(std::max(0, int32_t(_loopStart.get().payload.intValue)));

    // Validate sustain index - clamp to valid range or -1 (no sustain)
    if (_sustainIdx >= int32_t(_stagesCache.size())) {
      _sustainIdx = -1;  // Treat as no-sustain if out of bounds
    }

    // Validate loop start index - clamp to valid range
    if (_looping && !_stagesCache.empty() && _loopStartIdx >= _stagesCache.size()) {
      _loopStartIdx = 0;  // Default to start if out of bounds
    }
  }

  // Curve interpolation: t in [0,1], curve in [-1,1]
  // curve < 0: logarithmic (fast start, slow end)
  // curve = 0: linear
  // curve > 0: exponential (slow start, fast end)
  // Internally uses power function with exponent range [1.0, 4.0] for musical response:
  //   curve=-1 → t^(-2) equivalent (fast attack)
  //   curve=0  → t^1 (linear)
  //   curve=1  → t^4 (slow attack, fast end)
  static inline float applyCurve(float t, float curve) {
    if (std::abs(curve) < 0.001f) {
      return t;  // Linear
    }
    if (curve > 0.0f) {
      // Exponential: slow start, fast end
      // Using power function: t^(1 + curve*3) gives range from t^1 to t^4
      return std::pow(t, 1.0f + curve * 3.0f);
    } else {
      // Logarithmic: fast start, slow end
      // Inverse of exponential: 1 - (1-t)^(1 + |curve|*3)
      return 1.0f - std::pow(1.0f - t, 1.0f - curve * 3.0f);
    }
  }

  // Process single channel (scalar)
  void processChannel(const float *gate, float *out, uint32_t nsamples, uint32_t sampleRate, uint32_t ch) {
    ChannelState &state = _channelStates[ch];

    for (uint32_t i = 0; i < nsamples; i++) {
      bool gateOn = gate[i] > 0.0f;
      bool risingEdge = gateOn && !state.prevGate;  // Gate just turned on
      bool fallingEdge = !gateOn && state.prevGate;  // Gate just turned off

      // Handle gate transitions
      if (risingEdge) {
        // Rising edge detected - start/restart attack from current level
        // This handles retrigger from any phase (Idle, Attack, Sustain, Release)
        state.phase = ChannelState::Phase::Attack;
        state.currentStage = 0;
        state.startLevel = state.level;  // Start from current level (no discontinuity)
        state.stageSamples = 0;
        state.totalStageSamples = 1;  // Default to 1 sample if empty
        if (!_stagesCache.empty()) {
          state.totalStageSamples =
              std::max(uint64_t(1), uint64_t(std::round(_stagesCache[0].time * sampleRate)));
        }
      } else if (fallingEdge && (state.phase == ChannelState::Phase::Attack || state.phase == ChannelState::Phase::Sustain)) {
        // Falling edge - start release
        state.phase = ChannelState::Phase::Release;
        state.currentStage = 0;
        state.startLevel = state.level;
        state.stageSamples = 0;
        state.totalStageSamples = 1;  // Default to 1 sample if empty
        if (!_releaseCache.empty()) {
          state.totalStageSamples =
              std::max(uint64_t(1), uint64_t(std::round(_releaseCache[0].time * sampleRate)));
        }
      }

      state.prevGate = gateOn;

      // Process current phase
      switch (state.phase) {
      case ChannelState::Phase::Idle:
        state.level = 0.0;
        break;

      case ChannelState::Phase::Attack: {
        if (_stagesCache.empty() || state.currentStage >= _stagesCache.size()) {
          // Stages reduced dynamically or empty - go to sustain at current level
          state.phase = ChannelState::Phase::Sustain;
          break;
        }

        const auto &stage = _stagesCache[state.currentStage];
        float t = float(state.stageSamples) / float(state.totalStageSamples);
        t = std::clamp(t, 0.0f, 1.0f);

        float curvedT = applyCurve(t, stage.curve);
        state.level = state.startLevel + (double(stage.level) - state.startLevel) * curvedT;

        state.stageSamples++;

        // Check for stage completion
        if (state.stageSamples >= state.totalStageSamples) {
          state.level = stage.level;

          // Check for sustain
          if (_sustainIdx >= 0 && state.currentStage == uint32_t(_sustainIdx)) {
            state.phase = ChannelState::Phase::Sustain;
          } else {
            // Move to next stage
            state.currentStage++;

            if (state.currentStage >= _stagesCache.size()) {
              // End of stages
              if (_looping && _loopStartIdx < _stagesCache.size()) {
                // Loop back
                state.currentStage = _loopStartIdx;
                state.startLevel = state.level;
                state.stageSamples = 0;
                state.totalStageSamples =
                    std::max(uint64_t(1), uint64_t(std::round(_stagesCache[state.currentStage].time * sampleRate)));
              } else {
                // Stay at final level (no sustain index set)
                state.phase = ChannelState::Phase::Sustain;
              }
            } else {
              state.startLevel = state.level;
              state.stageSamples = 0;
              state.totalStageSamples =
                  std::max(uint64_t(1), uint64_t(std::round(_stagesCache[state.currentStage].time * sampleRate)));
            }
          }
        }
        break;
      }

      case ChannelState::Phase::Sustain:
        // Level stays constant until gate off
        break;

      case ChannelState::Phase::Release: {
        if (_releaseCache.empty() || state.currentStage >= _releaseCache.size()) {
          // Release stages reduced dynamically or empty - go to idle
          state.phase = ChannelState::Phase::Idle;
          state.level = 0.0;
          break;
        }

        const auto &stage = _releaseCache[state.currentStage];
        float t = float(state.stageSamples) / float(state.totalStageSamples);
        t = std::clamp(t, 0.0f, 1.0f);

        float curvedT = applyCurve(t, stage.curve);
        state.level = state.startLevel + (double(stage.level) - state.startLevel) * curvedT;

        state.stageSamples++;

        // Check for stage completion
        if (state.stageSamples >= state.totalStageSamples) {
          state.level = stage.level;
          state.currentStage++;

          if (state.currentStage >= _releaseCache.size()) {
            // Release complete
            state.phase = ChannelState::Phase::Idle;
            state.level = 0.0;
          } else {
            state.startLevel = state.level;
            state.stageSamples = 0;
            state.totalStageSamples =
                std::max(uint64_t(1), uint64_t(std::round(_releaseCache[state.currentStage].time * sampleRate)));
          }
        }
        break;
      }
      }

      out[i] = float(state.level);
    }
  }

  // Process 4 channels - batched for cache-friendly access patterns
  // Note: Envelope state machine logic is inherently serial per-channel,
  // so we process channels independently rather than true SIMD vectorization
  void processChannelsBatch(const float *gate0, const float *gate1, const float *gate2, const float *gate3, float *out0,
                            float *out1, float *out2, float *out3, uint32_t nsamples, uint32_t sampleRate,
                            uint32_t chBase) {
    processChannel(gate0, out0, nsamples, sampleRate, chBase);
    processChannel(gate1, out1, nsamples, sampleRate, chBase + 1);
    processChannel(gate2, out2, nsamples, sampleRate, chBase + 2);
    processChannel(gate3, out3, nsamples, sampleRate, chBase + 3);
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    const auto &audio = input.payload.audioValue;
    const uint32_t nsamples = audio.nsamples;
    const uint32_t sampleRate = audioGetSampleRate(audio);
    const uint32_t channels = audio.channels;

    // Validate buffer size
    if (nsamples > MAX_SAMPLES || channels > MAX_CHANNELS) {
      throw ActivationError("Audio.MultiStageEnvelope: buffer size exceeds limits");
    }

    // Re-cache stages each frame to support dynamic parameter changes
    cacheStages();

    _buffer.resize(channels * nsamples);

    uint32_t ch = 0;

    // Process groups of 4 channels for cache-friendly access
    for (; ch + 4 <= channels; ch += 4) {
      const float *gate0 = audio.samples + ch * nsamples;
      const float *gate1 = audio.samples + (ch + 1) * nsamples;
      const float *gate2 = audio.samples + (ch + 2) * nsamples;
      const float *gate3 = audio.samples + (ch + 3) * nsamples;

      float *out0 = _buffer.data() + ch * nsamples;
      float *out1 = _buffer.data() + (ch + 1) * nsamples;
      float *out2 = _buffer.data() + (ch + 2) * nsamples;
      float *out3 = _buffer.data() + (ch + 3) * nsamples;

      processChannelsBatch(gate0, gate1, gate2, gate3, out0, out1, out2, out3, nsamples, sampleRate, ch);
    }

    // Process remaining channels with scalar code
    for (; ch < channels; ch++) {
      const float *gateSamples = audio.samples + ch * nsamples;
      float *outSamples = _buffer.data() + ch * nsamples;
      processChannel(gateSamples, outSamples, nsamples, sampleRate, ch);
    }

    return Var(makeAudio(_buffer.data(), nsamples, sampleRate, uint8_t(channels)));
  }
};

// =============================================================================
// State Variable Filter (SVF) - with SIMD multi-channel processing
// =============================================================================

struct Filter {
  enum class FilterType { Lowpass, Highpass, Bandpass, Notch };
  DECL_ENUM_INFO(FilterType, FilterType, "Type of filter response.", 'filt');

  // Parameters
  PARAM_VAR(_type, "Type", "Filter type (Lowpass, Highpass, Bandpass, Notch).", {FilterTypeEnumInfo::Type});
  PARAM_PARAMVAR(_cutoff, "Cutoff", "Cutoff frequency in Hz.", {CoreInfo::FloatType, CoreInfo::FloatVarType});
  PARAM_PARAMVAR(_resonance, "Resonance", "Resonance amount (0.0 to 1.0).", {CoreInfo::FloatType, CoreInfo::FloatVarType});

  PARAM_IMPL(PARAM_IMPL_FOR(_type), PARAM_IMPL_FOR(_cutoff), PARAM_IMPL_FOR(_resonance));

  // SVF state stored as aligned arrays for SIMD access
  // We process up to 4 channels in parallel using SIMD
  static constexpr uint32_t SIMD_WIDTH = 4;

  // State arrays aligned for SIMD (padded to multiple of SIMD_WIDTH)
  alignas(16) std::array<float, MAX_CHANNELS> _ic1eq{};
  alignas(16) std::array<float, MAX_CHANNELS> _ic2eq{};

  std::vector<float> _buffer;

  Filter() {
    _type = Var::Enum(FilterType::Lowpass, CoreCC, FilterTypeEnumInfo::TypeId);
    _cutoff = Var(1000.0);   // 1kHz default cutoff
    _resonance = Var(0.5);   // 50% resonance
  }

  static SHOptionalString help() {
    return SHCCSTR("State Variable Filter (SVF) with Lowpass, Highpass, Bandpass, and Notch modes. "
                   "Cutoff and Resonance can be modulated for filter sweeps and effects. "
                   "Uses SIMD acceleration for multi-channel audio.");
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

    // Reset filter states
    _ic1eq.fill(0.0f);
    _ic2eq.fill(0.0f);
  }

  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);
  }

#if defined(SYNTH_HAS_NEON)
  // NEON SIMD: Process 4 channels simultaneously for one sample
  // Input: v0 contains sample i from channels 0-3
  // Output: writes to out channels 0-3 at sample i
  template <FilterType Type>
  inline void processSVF_NEON_4ch(const float *in0, const float *in1, const float *in2, const float *in3,
                                   float *out0, float *out1, float *out2, float *out3,
                                   uint32_t nsamples, float g, float k, uint32_t chBase) {
    // Load state for 4 channels
    float32x4_t ic1 = vld1q_f32(&_ic1eq[chBase]);
    float32x4_t ic2 = vld1q_f32(&_ic2eq[chBase]);

    // Precompute constants
    float32x4_t vg = vdupq_n_f32(g);
    float32x4_t vk = vdupq_n_f32(k);
    float32x4_t v2 = vdupq_n_f32(2.0f);
    float32x4_t denom = vdupq_n_f32(1.0f / (1.0f + g * (g + k)));

    // Temp buffer for gather operation (outside loop to avoid repeated stack allocation)
    alignas(16) float tmp[4];

    for (uint32_t i = 0; i < nsamples; i++) {
      // Gather sample i from each channel
      tmp[0] = in0[i]; tmp[1] = in1[i]; tmp[2] = in2[i]; tmp[3] = in3[i];
      float32x4_t v0 = vld1q_f32(tmp);

      // SVF equations (all 4 channels in parallel):
      // v1 = (ic1eq + g * (v0 - ic2eq)) / (1 + g * (g + k))
      float32x4_t v1 = vmulq_f32(vaddq_f32(ic1, vmulq_f32(vg, vsubq_f32(v0, ic2))), denom);

      // v2_out = ic2eq + g * v1
      float32x4_t v2_out = vaddq_f32(ic2, vmulq_f32(vg, v1));

      // Update state: ic1eq = 2*v1 - ic1eq, ic2eq = 2*v2 - ic2eq
      ic1 = vsubq_f32(vmulq_f32(v2, v1), ic1);
      ic2 = vsubq_f32(vmulq_f32(v2, v2_out), ic2);

      // Compute output based on filter type
      float32x4_t result;
      if constexpr (Type == FilterType::Lowpass) {
        result = v2_out;
      } else if constexpr (Type == FilterType::Highpass) {
        // hp = v0 - k*v1 - v2
        result = vsubq_f32(vsubq_f32(v0, vmulq_f32(vk, v1)), v2_out);
      } else if constexpr (Type == FilterType::Bandpass) {
        result = v1;
      } else if constexpr (Type == FilterType::Notch) {
        // notch = lp + hp = v2 + (v0 - k*v1 - v2) = v0 - k*v1
        result = vsubq_f32(v0, vmulq_f32(vk, v1));
      }

      // Scatter result to each channel's output buffer
      out0[i] = vgetq_lane_f32(result, 0);
      out1[i] = vgetq_lane_f32(result, 1);
      out2[i] = vgetq_lane_f32(result, 2);
      out3[i] = vgetq_lane_f32(result, 3);
    }

    // Store state back
    vst1q_f32(&_ic1eq[chBase], ic1);
    vst1q_f32(&_ic2eq[chBase], ic2);
  }
#endif

#if defined(SYNTH_HAS_SSE)
  // SSE SIMD: Process 4 channels simultaneously for one sample
  template <FilterType Type>
  inline void processSVF_SSE_4ch(const float *in0, const float *in1, const float *in2, const float *in3,
                                  float *out0, float *out1, float *out2, float *out3,
                                  uint32_t nsamples, float g, float k, uint32_t chBase) {
    // Load state for 4 channels
    __m128 ic1 = _mm_load_ps(&_ic1eq[chBase]);
    __m128 ic2 = _mm_load_ps(&_ic2eq[chBase]);

    // Precompute constants
    __m128 vg = _mm_set1_ps(g);
    __m128 vk = _mm_set1_ps(k);
    __m128 v2 = _mm_set1_ps(2.0f);
    __m128 denom = _mm_set1_ps(1.0f / (1.0f + g * (g + k)));

    for (uint32_t i = 0; i < nsamples; i++) {
      // Gather sample i from each channel
      __m128 v0 = _mm_set_ps(in3[i], in2[i], in1[i], in0[i]);

      // SVF equations:
      // v1 = (ic1eq + g * (v0 - ic2eq)) / (1 + g * (g + k))
      __m128 v1 = _mm_mul_ps(_mm_add_ps(ic1, _mm_mul_ps(vg, _mm_sub_ps(v0, ic2))), denom);

      // v2_out = ic2eq + g * v1
      __m128 v2_out = _mm_add_ps(ic2, _mm_mul_ps(vg, v1));

      // Update state
      ic1 = _mm_sub_ps(_mm_mul_ps(v2, v1), ic1);
      ic2 = _mm_sub_ps(_mm_mul_ps(v2, v2_out), ic2);

      // Compute output based on filter type
      __m128 result;
      if constexpr (Type == FilterType::Lowpass) {
        result = v2_out;
      } else if constexpr (Type == FilterType::Highpass) {
        result = _mm_sub_ps(_mm_sub_ps(v0, _mm_mul_ps(vk, v1)), v2_out);
      } else if constexpr (Type == FilterType::Bandpass) {
        result = v1;
      } else if constexpr (Type == FilterType::Notch) {
        result = _mm_sub_ps(v0, _mm_mul_ps(vk, v1));
      }

      // Scatter result to each channel's output buffer
      alignas(16) float tmp[4];
      _mm_store_ps(tmp, result);
      out0[i] = tmp[0];
      out1[i] = tmp[1];
      out2[i] = tmp[2];
      out3[i] = tmp[3];
    }

    // Store state back
    _mm_store_ps(&_ic1eq[chBase], ic1);
    _mm_store_ps(&_ic2eq[chBase], ic2);
  }
#endif

  // Scalar fallback: process single channel
  template <FilterType Type>
  inline void processSVF_Scalar(const float *in, float *out, uint32_t nsamples, float g, float k, uint32_t ch) {
    float ic1 = _ic1eq[ch];
    float ic2 = _ic2eq[ch];
    const float denom = 1.0f / (1.0f + g * (g + k));

    for (uint32_t i = 0; i < nsamples; i++) {
      float v0 = in[i];

      // SVF equations
      float v1 = (ic1 + g * (v0 - ic2)) * denom;
      float v2_out = ic2 + g * v1;

      // Update state
      ic1 = 2.0f * v1 - ic1;
      ic2 = 2.0f * v2_out - ic2;

      // Output based on filter type
      if constexpr (Type == FilterType::Lowpass) {
        out[i] = v2_out;
      } else if constexpr (Type == FilterType::Highpass) {
        out[i] = v0 - k * v1 - v2_out;
      } else if constexpr (Type == FilterType::Bandpass) {
        out[i] = v1;
      } else if constexpr (Type == FilterType::Notch) {
        out[i] = v0 - k * v1;
      }
    }

    _ic1eq[ch] = ic1;
    _ic2eq[ch] = ic2;
  }

  template <FilterType Type> SHVar processFilter(SHContext *context, const SHVar &input) {
    const auto &audio = input.payload.audioValue;
    const uint32_t nsamples = audio.nsamples;
    const uint32_t sampleRate = audioGetSampleRate(audio);
    const uint32_t channels = audio.channels;

    // Get parameters
    // Clamp cutoff to 0.45 * Nyquist to prevent numerical instability from tan() approaching infinity
    // At 0.45, tan(π * 0.45) ≈ 3.08 which is well-behaved. At 0.49, tan(π * 0.49) ≈ 27.3
    const float cutoff = float(std::clamp(_cutoff.get().payload.floatValue, 20.0, double(sampleRate) * 0.45));
    const float resonance = float(std::clamp(_resonance.get().payload.floatValue, 0.0, 1.0));

    // SVF coefficients (float for SIMD)
    const float g = std::tan(float(M_PI) * cutoff / float(sampleRate));
    // k controls damping: k=2 is critically damped, k approaching 0 gives self-oscillation.
    // We allow resonance up to 0.99 which gives k=0.02, allowing near self-oscillation
    // for creative filter effects (acid bass lines, etc). The filter remains stable.
    const float k = 2.0f - 2.0f * resonance * 0.99f;

    // Validate buffer size
    if (nsamples > MAX_SAMPLES || channels > MAX_CHANNELS) {
      throw ActivationError("Audio.Filter: buffer size exceeds limits");
    }

    _buffer.resize(channels * nsamples);

    uint32_t ch = 0;

#if defined(SYNTH_HAS_NEON) || defined(SYNTH_HAS_SSE)
    // Process groups of 4 channels using SIMD
    for (; ch + 4 <= channels; ch += 4) {
      const float *in0 = audio.samples + ch * nsamples;
      const float *in1 = audio.samples + (ch + 1) * nsamples;
      const float *in2 = audio.samples + (ch + 2) * nsamples;
      const float *in3 = audio.samples + (ch + 3) * nsamples;

      float *out0 = _buffer.data() + ch * nsamples;
      float *out1 = _buffer.data() + (ch + 1) * nsamples;
      float *out2 = _buffer.data() + (ch + 2) * nsamples;
      float *out3 = _buffer.data() + (ch + 3) * nsamples;

#if defined(SYNTH_HAS_NEON)
      processSVF_NEON_4ch<Type>(in0, in1, in2, in3, out0, out1, out2, out3, nsamples, g, k, ch);
#elif defined(SYNTH_HAS_SSE)
      processSVF_SSE_4ch<Type>(in0, in1, in2, in3, out0, out1, out2, out3, nsamples, g, k, ch);
#endif
    }
#endif

    // Process remaining channels with scalar code
    for (; ch < channels; ch++) {
      const float *inSamples = audio.samples + ch * nsamples;
      float *outSamples = _buffer.data() + ch * nsamples;
      processSVF_Scalar<Type>(inSamples, outSamples, nsamples, g, k, ch);
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
  REGISTER_SHARD("Audio.MultiStageEnvelope", MultiStageEnvelope);
  REGISTER_SHARD("Audio.Filter", Filter);
}
