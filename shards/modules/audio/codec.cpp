/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2025 Fragcolor Pte. Ltd. */

#include <shards/core/shared.hpp>
#include <shards/core/runtime.hpp>
#include <shards/core/params.hpp>
#include <shards/core/platform.hpp>
#include <shards/log/log.hpp>
#include <opus.h>
#include <opus_multistream.h>
#include <vector>
#include <cstring>

namespace shards {
namespace Audio {

struct Compress {
  static SHOptionalString help() {
    return SHCCSTR("Compresses audio data using the Opus codec. Opus is designed for interactive speech and audio "
                   "transmission over the Internet, providing high-quality compression with low latency. "
                   "This shard takes raw audio data and outputs compressed Opus packets as a sequence of byte chunks.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::AudioType; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("Accepts audio data as an Audio chunk, containing the sample rate, number of samples, "
                   "number of channels, and the audio samples. Opus supports sample rates of 8, 12, 16, 24, or 48 kHz.");
  }
  static SHTypesInfo outputTypes() { return CoreInfo::BytesSeqType; }
  static SHOptionalString outputHelp() {
    return SHCCSTR("Outputs the compressed audio data as a sequence of Binary chunks, each containing an Opus-encoded packet.");
  }

  // Bitrate: Target bitrate in bits per second (bps)
  PARAM_PARAMVAR(_bitrate, "Bitrate",
                 "The target bitrate in bits per second (bps). Valid values range from 500 to 512000. "
                 "Use 0 for automatic bitrate management.",
                 {CoreInfo::IntType, CoreInfo::IntVarType});

  // Application: Type of audio being encoded
  PARAM_PARAMVAR(_application, "Application",
                 "The type of audio being encoded. Options are: \"voip\" (optimize for speech), "
                 "\"audio\" (optimize for general audio), or \"lowdelay\" (optimize for low latency).",
                 {CoreInfo::StringType, CoreInfo::StringVarType});

  // Complexity: Encoding complexity (0-10)
  PARAM_PARAMVAR(
      _complexity, "Complexity",
      "The encoding complexity, from 0 (lowest) to 10 (highest). Higher values use more CPU but may provide better quality.",
      {CoreInfo::IntType, CoreInfo::IntVarType});

  // FrameSize: Frame size in frames (samples)
  PARAM_PARAMVAR(_frameSize, "FrameSize",
                 "The frame size in frames (samples). Must be one of the valid Opus frame sizes for the given sample rate. "
                 "Common values are 120, 240, 480, 960, 1920, or 2880 frames at 48kHz.",
                 {CoreInfo::IntType, CoreInfo::IntVarType});

  // UseMultistream: Enable multistream encoding for >2 channels
  PARAM_PARAMVAR(_useMultistream, "UseMultistream", "Enable if the source is multistream encoded.",
                 {CoreInfo::BoolType, CoreInfo::BoolVarType});

  PARAM_IMPL(PARAM_IMPL_FOR(_bitrate), PARAM_IMPL_FOR(_application), PARAM_IMPL_FOR(_complexity), PARAM_IMPL_FOR(_frameSize),
             PARAM_IMPL_FOR(_useMultistream));

  void setup() {
    _bitrate = Var(64000);        // 64 kbps default bitrate
    _application = Var("audio");  // Default to general audio
    _complexity = Var(10);        // Default to highest complexity
    _frameSize = Var(960);        // 20ms at 48kHz (960 frames)
    _useMultistream = Var(false); // Default to basic Opus
  }

  PARAM_REQUIRED_VARIABLES()
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _encoder = nullptr;
    _msEncoder = nullptr;
    _encoderInitialized = false;
    _isMultistream = false;
    _inputBuffer.clear();
    _outputPackets.clear();
  }

  void cleanup(SHContext *context) {
    if (_encoder) {
      opus_encoder_destroy(_encoder);
      _encoder = nullptr;
    }
    if (_msEncoder) {
      opus_multistream_encoder_destroy(_msEncoder);
      _msEncoder = nullptr;
    }
    _encoderInitialized = false;
    _isMultistream = false;
    PARAM_CLEANUP(context);
  }

  // Initialize or reinitialize the encoder if needed
  bool initializeEncoder(int sampleRate, int channels) {
    bool useMultistream = _useMultistream.get().payload.boolValue || channels > 2;

    // Check if we need to reinitialize the encoder
    if ((_encoder || _msEncoder) &&
        (_lastSampleRate != sampleRate || _lastChannels != channels || _isMultistream != useMultistream)) {
      if (_encoder) {
        opus_encoder_destroy(_encoder);
        _encoder = nullptr;
      }
      if (_msEncoder) {
        opus_multistream_encoder_destroy(_msEncoder);
        _msEncoder = nullptr;
      }
      _encoderInitialized = false;
    }

    if (!_encoder && !_msEncoder) {
      int error = 0;
      int app = OPUS_APPLICATION_AUDIO; // Default

      // Convert application string to Opus application type
      std::string appStr = _application.get().payload.stringValue;
      if (appStr == "voip") {
        app = OPUS_APPLICATION_VOIP;
      } else if (appStr == "audio") {
        app = OPUS_APPLICATION_AUDIO;
      } else if (appStr == "lowdelay") {
        app = OPUS_APPLICATION_RESTRICTED_LOWDELAY;
      } else {
        SHLOG_ERROR("Invalid application type: {}. Using 'audio' instead.", appStr);
      }

      if (useMultistream) {
        // Create multistream encoder
        _isMultistream = true;

        // Set up channel mapping (simple mapping for now)
        _channelMapping.resize(channels);
        int streams = 0;
        int coupled_streams = 0;

        if (channels == 1) {
          streams = 1;
          coupled_streams = 0;
          _channelMapping[0] = 0;
        } else if (channels == 2) {
          streams = 1;
          coupled_streams = 1;
          _channelMapping[0] = 0;
          _channelMapping[1] = 1;
        } else {
          // For >2 channels, use simple mapping (each channel gets its own stream)
          streams = channels;
          coupled_streams = 0;
          for (int i = 0; i < channels; i++) {
            _channelMapping[i] = i;
          }
        }

        _msEncoder =
            opus_multistream_encoder_create(sampleRate, channels, streams, coupled_streams, _channelMapping.data(), app, &error);
        if (error != OPUS_OK || !_msEncoder) {
          SHLOG_ERROR("Failed to create Opus multistream encoder: {}", opus_strerror(error));
          return false;
        }

        // Set encoder parameters
        int bitrate = _bitrate.get().payload.intValue;
        opus_multistream_encoder_ctl(_msEncoder, OPUS_SET_BITRATE(bitrate));

        int complexity = _complexity.get().payload.intValue;
        opus_multistream_encoder_ctl(_msEncoder, OPUS_SET_COMPLEXITY(complexity));

      } else {
        // Create basic encoder (1-2 channels only)
        _isMultistream = false;

        if (channels > 2) {
          SHLOG_ERROR("Basic Opus encoder supports max 2 channels. Use UseMultistream=true for {} channels.", channels);
          return false;
        }

        _encoder = opus_encoder_create(sampleRate, channels, app, &error);
        if (error != OPUS_OK || !_encoder) {
          SHLOG_ERROR("Failed to create Opus encoder: {}", opus_strerror(error));
          return false;
        }

        // Set encoder parameters
        int bitrate = _bitrate.get().payload.intValue;
        opus_encoder_ctl(_encoder, OPUS_SET_BITRATE(bitrate));

        int complexity = _complexity.get().payload.intValue;
        opus_encoder_ctl(_encoder, OPUS_SET_COMPLEXITY(complexity));
      }

      _lastSampleRate = sampleRate;
      _lastChannels = channels;
      _encoderInitialized = true;
    }

    return _encoderInitialized;
  }

  // Validate frame size in samples for the given sample rate
  bool isValidFrameSize(int frameSizeSamples, int sampleRate) {
    // Valid Opus frame sizes are 2.5, 5, 10, 20, 40, or 60 ms
    const float validFrameSizesMs[] = {2.5f, 5.0f, 10.0f, 20.0f, 40.0f, 60.0f};

    for (float validMs : validFrameSizesMs) {
      // Calculate the exact number of samples for this valid ms value
      int validSamples = static_cast<int>(validMs * sampleRate / 1000.0f);

      // Allow a small tolerance for floating-point calculations
      if (std::abs(frameSizeSamples - validSamples) < 2) {
        return true;
      }
    }

    return false;
  }

  OpusEncoder *_encoder = nullptr;
  OpusMSEncoder *_msEncoder = nullptr; // Multistream encoder
  bool _encoderInitialized = false;
  bool _isMultistream = false;
  int _lastSampleRate = 0;
  int _lastChannels = 0;
  std::vector<unsigned char> _channelMapping; // Channel mapping for multistream
  std::vector<float> _inputBuffer;            // Buffer to accumulate input samples
  std::vector<unsigned char> _encodedBuffer;  // Buffer for encoded data
  std::vector<SHVar> _outputPackets;          // Sequence of encoded packets

  std::vector<float> _interleavedInput; // Buffer for interleaved audio (Opus expects interleaved)

  SHVar activate(SHContext *context, const SHVar &input) {
    const auto &audio = input.payload.audioValue;
    int sampleRate = static_cast<int>(audioGetSampleRate(audio));
    int numChannels = static_cast<int>(audio.channels);
    int numSamples = static_cast<int>(audio.nsamples);

    // Validate sample rate (Opus supports 8, 12, 16, 24, or 48 kHz)
    if (sampleRate != 8000 && sampleRate != 12000 && sampleRate != 16000 && sampleRate != 24000 && sampleRate != 48000) {
      auto msg = fmt::format("Unsupported sample rate: {}. Opus supports 8, 12, 16, 24, or 48 kHz.", sampleRate);
      throw ActivationError(msg);
    }

    // Initialize or reinitialize the encoder if needed
    if (!initializeEncoder(sampleRate, numChannels)) {
      auto msg = fmt::format("Failed to initialize Opus encoder");
      throw ActivationError(msg);
    }

    // Get frame size in samples
    int frameSizeSamples = _frameSize.get().payload.intValue;

    // Validate frame size
    if (frameSizeSamples <= 0) {
      auto msg = fmt::format("Invalid frame size: {} samples", frameSizeSamples);
      throw ActivationError(msg);
    }

    // Ensure frame size is valid for Opus
    if (!isValidFrameSize(frameSizeSamples, sampleRate)) {
      // Calculate some valid frame sizes for error message
      int valid2_5ms = static_cast<int>(2.5f * sampleRate / 1000.0f);
      int valid5ms = static_cast<int>(5.0f * sampleRate / 1000.0f);
      int valid10ms = static_cast<int>(10.0f * sampleRate / 1000.0f);
      int valid20ms = static_cast<int>(20.0f * sampleRate / 1000.0f);

      auto msg =
          fmt::format("Invalid frame size for Opus: {} samples at {} Hz. Valid frame sizes are approximately: {}, {}, {}, {}, "
                      "{}, {} samples.",
                      frameSizeSamples, sampleRate, valid2_5ms, valid5ms, valid10ms, valid20ms, valid20ms * 2, valid20ms * 3);
      throw ActivationError(msg);
    }

    // Opus expects interleaved audio, so convert planar to interleaved
    _interleavedInput.resize(numSamples * numChannels);
    audioInterleave(audio.samples, _interleavedInput.data(), uint32_t(numSamples), uint8_t(numChannels));

    // Append new audio data to our input buffer
    size_t oldSize = _inputBuffer.size();
    _inputBuffer.resize(oldSize + numSamples * numChannels);
    std::memcpy(_inputBuffer.data() + oldSize, _interleavedInput.data(), numSamples * numChannels * sizeof(float));

    // Clear output packets for this activation
    _outputPackets.clear();

    // Resize the encoding buffer (maximum size per frame is 1276 bytes as per Opus docs)
    _encodedBuffer.resize(1276);

    // Process complete frames from the input buffer
    while (_inputBuffer.size() >= frameSizeSamples * size_t(numChannels)) {
      // Encode the frame using appropriate encoder
      opus_int32 encodedBytes;

      if (_isMultistream) {
        encodedBytes = opus_multistream_encode_float(_msEncoder, _inputBuffer.data(), frameSizeSamples, _encodedBuffer.data(),
                                                     static_cast<opus_int32>(_encodedBuffer.size()));
      } else {
        encodedBytes = opus_encode_float(_encoder, _inputBuffer.data(), frameSizeSamples, _encodedBuffer.data(),
                                         static_cast<opus_int32>(_encodedBuffer.size()));
      }

      if (encodedBytes < 0) {
        auto msg = fmt::format("Opus encoding failed: {}", opus_strerror(encodedBytes));
        throw ActivationError(msg);
      }

      // Create a new packet and add it to the output sequence
      if (encodedBytes > 0) {
        // Create a copy of the encoded data for this packet
        _outputPackets.push_back(Var(_encodedBuffer.data(), static_cast<uint32_t>(encodedBytes)));
      }

      // Remove the processed frame from the input buffer
      size_t frameSize = frameSizeSamples * numChannels;
      if (_inputBuffer.size() > frameSize) {
        std::memmove(_inputBuffer.data(), _inputBuffer.data() + frameSize, (_inputBuffer.size() - frameSize) * sizeof(float));
        _inputBuffer.resize(_inputBuffer.size() - frameSize);
      } else {
        _inputBuffer.clear();
      }
    }

    // Create output sequence with compressed packets
    return Var(_outputPackets);
  }
};

struct Decompress {
  static SHOptionalString help() {
    return SHCCSTR("Decompresses audio data that was encoded with the Opus codec. This shard takes Opus-encoded "
                   "binary data and outputs raw audio samples. Opus is designed for interactive speech and audio "
                   "transmission over the Internet, providing high-quality compression with low latency.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::BytesType; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("Accepts compressed audio data as a Binary chunk containing Opus-encoded data.");
  }
  static SHTypesInfo outputTypes() { return CoreInfo::AudioType; }
  static SHOptionalString outputHelp() { return SHCCSTR("Outputs the decompressed audio data as an Audio chunk."); }

  // SampleRate: Output sample rate
  PARAM_PARAMVAR(_sampleRate, "SampleRate", "The sample rate for the output audio in Hz. Opus supports 8, 12, 16, 24, or 48 kHz.",
                 {CoreInfo::IntType, CoreInfo::IntVarType});

  // Channels: Number of audio channels
  PARAM_PARAMVAR(_channels, "Channels", "The number of audio channels (1 for mono, 2 for stereo, up to 255 for multistream).",
                 {CoreInfo::IntType, CoreInfo::IntVarType});

  // UseMultistream: Enable multistream encoding for >2 channels
  PARAM_PARAMVAR(_useMultistream, "UseMultistream", "Enable multistream encoding for complex channel layouts (>2 channels).",
                 {CoreInfo::BoolType, CoreInfo::BoolVarType});

  // FrameSize: Frame size in frames (samples)
  PARAM_PARAMVAR(_frameSize, "FrameSize",
                 "The frame size in frames (samples). Must be one of the valid Opus frame sizes for the given sample rate. "
                 "Common values are 120, 240, 480, 960, 1920, or 2880 frames at 48kHz.",
                 {CoreInfo::IntType, CoreInfo::IntVarType});

  PARAM_IMPL(PARAM_IMPL_FOR(_sampleRate), PARAM_IMPL_FOR(_channels), PARAM_IMPL_FOR(_frameSize));

  void setup() {
    _sampleRate = Var(48000); // 48 kHz default sample rate
    _channels = Var(2);       // Stereo default
    _frameSize = Var(960);    // 20ms at 48kHz (960 frames)
  }

  PARAM_REQUIRED_VARIABLES()
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _decoder = nullptr;
    _decoderInitialized = false;
    _outputBuffer.clear();
  }

  void cleanup(SHContext *context) {
    if (_decoder) {
      opus_decoder_destroy(_decoder);
      _decoder = nullptr;
    }
    _decoderInitialized = false;
    PARAM_CLEANUP(context);
  }

  // Initialize or reinitialize the decoder if needed
  bool initializeDecoder(int sampleRate, int channels) {
    // Check if we need to reinitialize the decoder
    if (_decoder && (_lastSampleRate != sampleRate || _lastChannels != channels)) {
      opus_decoder_destroy(_decoder);
      _decoder = nullptr;
      _decoderInitialized = false;
    }

    if (!_decoder) {
      int error = 0;

      // Create the decoder
      _decoder = opus_decoder_create(sampleRate, channels, &error);
      if (error != OPUS_OK || !_decoder) {
        auto msg = fmt::format("Failed to create Opus decoder: {}", opus_strerror(error));
        throw ActivationError(msg);
      }

      _lastSampleRate = sampleRate;
      _lastChannels = channels;
      _decoderInitialized = true;
    }

    return _decoderInitialized;
  }

  // Validate frame size in samples for the given sample rate
  bool isValidFrameSize(int frameSizeSamples, int sampleRate) {

    // Valid Opus frame sizes are 2.5, 5, 10, 20, 40, or 60 ms
    const float validFrameSizesMs[] = {2.5f, 5.0f, 10.0f, 20.0f, 40.0f, 60.0f};

    for (float validMs : validFrameSizesMs) {
      // Calculate the exact number of samples for this valid ms value
      int validSamples = static_cast<int>(validMs * sampleRate / 1000.0f);

      // Allow a small tolerance for floating-point calculations
      if (std::abs(frameSizeSamples - validSamples) < 2) {
        return true;
      }
    }

    return false;
  }

  OpusDecoder *_decoder = nullptr;
  bool _decoderInitialized = false;
  int _lastSampleRate = 0;
  int _lastChannels = 0;
  std::vector<float> _interleavedOutput; // Opus outputs interleaved
  std::vector<float> _outputBuffer;      // Planar output

  SHVar activate(SHContext *context, const SHVar &input) {
    const auto &binaryData = input.payload.bytesValue;
    const auto binarySize = input.payload.bytesSize;
    if (!binaryData || binarySize == 0) {
      auto msg = fmt::format("Empty input data for Opus decompression");
      throw ActivationError(msg);
    }

    // Get parameters
    int sampleRate = _sampleRate.get().payload.intValue;
    int channels = _channels.get().payload.intValue;
    int frameSizeSamples = _frameSize.get().payload.intValue;

    // Validate sample rate (Opus supports 8, 12, 16, 24, or 48 kHz)
    if (sampleRate != 8000 && sampleRate != 12000 && sampleRate != 16000 && sampleRate != 24000 && sampleRate != 48000) {
      auto msg = fmt::format("Unsupported sample rate: {}. Opus supports 8, 12, 16, 24, or 48 kHz.", sampleRate);
      throw ActivationError(msg);
    }

    // Validate channels (1 or 2 for basic Opus)
    if (channels < 1 || channels > 2) {
      auto msg = fmt::format("Unsupported channel count: {}. Basic Opus supports 1 or 2 channels.", channels);
      throw ActivationError(msg);
    }

    // Initialize or reinitialize the decoder if needed
    if (!initializeDecoder(sampleRate, channels)) {
      auto msg = fmt::format("Failed to initialize Opus decoder");
      throw ActivationError(msg);
    }

    // Validate frame size
    if (frameSizeSamples <= 0) {
      auto msg = fmt::format("Invalid frame size: {} samples", frameSizeSamples);
      throw ActivationError(msg);
    }

    // Ensure frame size is valid for Opus
    if (!isValidFrameSize(frameSizeSamples, sampleRate)) {
      // Calculate some valid frame sizes for error message
      int valid2_5ms = static_cast<int>(2.5f * sampleRate / 1000.0f);
      int valid5ms = static_cast<int>(5.0f * sampleRate / 1000.0f);
      int valid10ms = static_cast<int>(10.0f * sampleRate / 1000.0f);
      int valid20ms = static_cast<int>(20.0f * sampleRate / 1000.0f);

      auto msg =
          fmt::format("Invalid frame size for Opus: {} samples at {} Hz. Valid frame sizes are approximately: {}, {}, {}, {}, "
                      "{}, {} samples.",
                      frameSizeSamples, sampleRate, valid2_5ms, valid5ms, valid10ms, valid20ms, valid20ms * 2, valid20ms * 3);
      throw ActivationError(msg);
    }

    // Resize output buffer to hold the decoded frame (Opus outputs interleaved)
    _interleavedOutput.resize(frameSizeSamples * channels);

    // Decode the Opus packet
    int samplesDecoded = opus_decode_float(_decoder, binaryData, static_cast<opus_int32>(binarySize), _interleavedOutput.data(),
                                           frameSizeSamples, 0);

    if (samplesDecoded < 0) {
      auto msg = fmt::format("Opus decoding failed: {}", opus_strerror(samplesDecoded));
      throw ActivationError(msg);
    }

    // Convert interleaved output to planar
    _outputBuffer.resize(samplesDecoded * channels);
    audioDeinterleave(_interleavedOutput.data(), _outputBuffer.data(), uint32_t(samplesDecoded), uint8_t(channels));

    // Create output audio with decompressed samples (planar format)
    return Var(makeAudio(_outputBuffer.data(), uint32_t(samplesDecoded), uint32_t(sampleRate), uint8_t(channels)));
  }
};

void registerCodecShards() {
  REGISTER_SHARD("Audio.Compress", Compress);
  REGISTER_SHARD("Audio.Decompress", Decompress);
}

} // namespace Audio
} // namespace shards