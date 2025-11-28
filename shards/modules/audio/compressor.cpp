/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2025 Fragcolor Pte. Ltd. */

#include <shards/core/shared.hpp>
#include <shards/core/runtime.hpp>
#include <shards/core/params.hpp>
#include <shards/core/platform.hpp>
#include <shards/log/log.hpp>
#include <algorithm>
#include <cmath>

namespace shards {
namespace Audio {

struct Compressor {
  static SHOptionalString help() {
    return SHCCSTR(
        "This shard applies dynamic range compression to audio data. It reduces the volume of loud sounds or amplifies "
        "quiet sounds by narrowing or 'compressing' an audio signal's dynamic range. The compressor is typically used "
        "within an Audio.Channel to control the dynamic range of audio signals, prevent clipping, and create a more "
        "consistent sound level.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::AudioType; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("Accepts audio data as an Audio chunk, containing the sample rate, number of samples, "
                   "number of channels, and the audio samples.");
  }
  static SHTypesInfo outputTypes() { return CoreInfo::AudioType; }
  static SHOptionalString outputHelp() { return SHCCSTR("Outputs the compressed audio data as an Audio chunk."); }

  // Threshold: Signal level above which compression is applied (in dB)
  PARAM_PARAMVAR(_threshold, "Threshold",
                 "The threshold level in dB below which the signal will pass unaffected. "
                 "Above this level, the signal will be compressed according to the ratio.",
                 {CoreInfo::FloatType, CoreInfo::FloatVarType});

  // Ratio: Amount of compression applied to signals above threshold (e.g., 4:1)
  PARAM_PARAMVAR(_ratio, "Ratio",
                 "The compression ratio (e.g., 4 means 4:1 compression). "
                 "Higher values result in more aggressive compression.",
                 {CoreInfo::FloatType, CoreInfo::FloatVarType});

  // Attack: Time it takes for compression to start after threshold is exceeded (in ms)
  PARAM_PARAMVAR(_attack, "Attack",
                 "The time in milliseconds it takes for the compressor to start reducing the gain after "
                 "the signal exceeds the threshold.",
                 {CoreInfo::FloatType, CoreInfo::FloatVarType});

  // Release: Time it takes for compression to stop after signal falls below threshold (in ms)
  PARAM_PARAMVAR(_release, "Release",
                 "The time in milliseconds it takes for the compressor to stop reducing the gain after "
                 "the signal falls below the threshold.",
                 {CoreInfo::FloatType, CoreInfo::FloatVarType});

  // Makeup gain: Additional gain applied to the compressed signal (in dB)
  PARAM_PARAMVAR(_makeupGain, "MakeupGain",
                 "Additional gain in dB applied to the compressed signal to compensate for the reduction in level.",
                 {CoreInfo::FloatType, CoreInfo::FloatVarType});

  PARAM_IMPL(PARAM_IMPL_FOR(_threshold), PARAM_IMPL_FOR(_ratio), PARAM_IMPL_FOR(_attack), PARAM_IMPL_FOR(_release),
             PARAM_IMPL_FOR(_makeupGain));

  void setup() {
    _threshold = Var(-24.0f); // -24 dB threshold
    _ratio = Var(4.0f);       // 4:1 ratio
    _attack = Var(5.0f);      // 5ms attack
    _release = Var(50.0f);    // 50ms release
    _makeupGain = Var(0.0f);  // 0dB makeup gain
  }

  PARAM_REQUIRED_VARIABLES()
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _envelope = 0.0f;
  }

  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  // Convert dB to linear amplitude
  inline float dbToLinear(float db) { return std::pow(10.0f, db / 20.0f); }

  // Convert linear amplitude to dB
  inline float linearToDb(float linear) { return 20.0f * std::log10(std::max(linear, 1e-6f)); }

  // Calculate time coefficient for attack/release
  inline float timeToCoeff(float timeMs, float sampleRate) { return std::exp(-1.0f / (timeMs * 0.001f * sampleRate)); }

  // Process buffer with compression
  std::vector<float> _buffer;
  float _envelope = 0.0f;

  SHVar activate(SHContext *context, const SHVar &input) {
    const auto &audio = input.payload.audioValue;
    uint32_t numSamples = audio.nsamples;
    uint32_t numChannels = audio.channels;
    uint32_t totalSamples = numSamples * numChannels;
    float sampleRate = static_cast<float>(audioGetSampleRate(audio));

    // Get parameter values
    float threshold = _threshold.get().payload.floatValue;
    float ratio = _ratio.get().payload.floatValue;
    float attack = _attack.get().payload.floatValue;
    float release = _release.get().payload.floatValue;
    float makeupGain = _makeupGain.get().payload.floatValue;

    // Convert parameters to appropriate units
    float thresholdLinear = dbToLinear(threshold);
    float makeupGainLinear = dbToLinear(makeupGain);
    float attackCoeff = timeToCoeff(attack, sampleRate);
    float releaseCoeff = timeToCoeff(release, sampleRate);
    float slope = 1.0f - (1.0f / ratio);

    // Resize buffer if needed (planar format)
    _buffer.resize(totalSamples);

    // Process each sample (audio is in planar format: [ch0_samples...][ch1_samples...])
    for (uint32_t i = 0; i < numSamples; ++i) {
      // Find the maximum absolute value across all channels for this sample
      float maxSample = 0.0f;
      for (uint32_t c = 0; c < numChannels; ++c) {
        // Planar: channel c sample i is at samples[c * numSamples + i]
        float sample = std::abs(audio.samples[c * numSamples + i]);
        maxSample = std::max(maxSample, sample);
      }

      // Envelope follower (peak detector with attack/release)
      if (maxSample > _envelope) {
        _envelope = attackCoeff * _envelope + (1.0f - attackCoeff) * maxSample;
      } else {
        _envelope = releaseCoeff * _envelope + (1.0f - releaseCoeff) * maxSample;
      }

      // Calculate gain reduction
      float gainReduction = 1.0f;
      if (_envelope > thresholdLinear) {
        float dbAboveThreshold = linearToDb(_envelope) - threshold;
        float dbGainReduction = -slope * dbAboveThreshold;
        gainReduction = dbToLinear(dbGainReduction);
      }

      // Apply gain reduction and makeup gain to all channels (planar format)
      for (uint32_t c = 0; c < numChannels; ++c) {
        uint32_t index = c * numSamples + i;
        _buffer[index] = audio.samples[index] * gainReduction * makeupGainLinear;
      }
    }

    // Create output audio with compressed samples (planar format)
    return Var(makeAudio(_buffer.data(), numSamples, uint32_t(sampleRate), uint8_t(numChannels)));
  }
};

void registerCompressorShards() { REGISTER_SHARD("Audio.Compressor", Compressor); }
} // namespace Audio
} // namespace shards