/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

#include <shards/core/shared.hpp>
#include <shards/core/runtime.hpp>
#include <shards/core/params.hpp>
#include <shards/core/platform.hpp>
#include <boost/lockfree/queue.hpp>
#include <shards/log/log.hpp>

#ifdef __clang__
#pragma clang attribute push(__attribute__((no_sanitize("undefined"))), apply_to = function)
#endif

#ifdef __APPLE__
#define MA_NO_RUNTIME_LINKING
#endif

// #ifndef NDEBUG
// #define MA_LOG_LEVEL MA_LOG_LEVEL_WARNING
// #define MA_DEBUG_OUTPUT 1
// #endif

#include <miniaudio/extras/stb_vorbis.c>
#undef R
#undef L
#undef C
#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#ifdef __clang__
#pragma clang attribute pop
#endif

#if SH_EMSCRIPTEN
#include <emscripten/threading.h>
#include <shards/core/em_proxy.hpp>
#endif

namespace shards {
namespace Audio {
static TableVar experimental{{Var("experimental"), Var(true)}};

/*

Inner audio wires should not be allowed to have (Pause) or clipping would
happen Also they should probably run like RunWire Detached so that multiple
references to the same wire would be possible and they would just produce
another iteration

*/

struct AudioDefaultHelpText {
  static inline constexpr const char *SoundObjectParam = "The sound object to manipulate";
};

struct ChannelData {
  float *outputBuffer;
  size_t outputBufferSize;
  std::vector<uint32_t> inChannels;
  std::vector<uint32_t> outChannels;
  ShardsVar shards;
  ParamVar volume{Var(1.0)};
  std::unordered_map<OwnedVar, OwnedVar> initialVariables;
};

struct ChannelDesc {
  uint32_t inBus;
  uint64_t inHash;
  uint32_t outBus;
  uint64_t outHash;
  uint32_t outChannels;
  ChannelData *data;
};

template <typename F> void proxyToMainThread(F &&cb) {
#if SH_EMSCRIPTEN
  if (emscripten_is_main_browser_thread()) {
    cb();
  } else {
    EmMainProxy::getInstance().queue(std::forward<F>(cb)).wait();
  }
#else
  cb();
#endif
}

struct Device {
  static constexpr uint32_t DeviceCC = 'sndd';

  static inline Type ObjType{{SHType::Object, {.object = {.vendorId = CoreCC, .typeId = DeviceCC}}}};

  // TODO add shards used as insert for the final mix
  static SHOptionalString help() {
    return SHCCSTR("This shard initializes an audio device in the mesh. This device sets up the audio context, manages input and "
                   "output channels, and handles the audio processing loop which is necessary for processing and playing audio "
                   "in the shards system. In essence, the Audio.Device provides the underlying audio system, and can be used "
                   "with other shards "
                   "like Audio.ReadFile and Audio.Channel to process and play audio.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString inputHelp() { return DefaultHelpText::InputHelpPass; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString outputHelp() { return DefaultHelpText::OutputHelpPass; }

  // Number of input channels for audio processing.
  PARAM_VAR(_inChannels, "InputChannels", "Sets the number of audio input channels for the device.", {CoreInfo::IntType});

  // Number of output channels for audio processing.
  PARAM_VAR(_outChannels, "OutputChannels", "Sets the number of audio output channels for the device.", {CoreInfo::IntType});

  // Sample rate of the audio device in Hertz (Hz). Defines how many samples per second the device processes. A value of 0 means
  // the device's default sample rate will be used.
  PARAM_VAR(_sampleRate, "SampleRate",
            "Specifies the sample rate of the audio device, in Hertz (Hz). A value of 0 means the device's default sample rate "
            "will be used.",
            {CoreInfo::IntType});

  // Size of the buffer used by the audio device. Note: Miniaudio may interpret this as a maximum value on certain platforms. A
  // value of 0 means the device's default buffer size will be used.
  PARAM_VAR(_bufferSize, "BufferSize",
            "Specifies the size of the audio buffer used by the device. Note: This may be interpreted as a maximum value on some "
            "platforms. A value of 0 means the device's default buffer size will be used.",
            {CoreInfo::IntType});

  PARAM_VAR(_deviceNameIn, "InputDevice",
            "An optional name of the input device to use, otherwise the default input device will be used.",
            {CoreInfo::NoneType, CoreInfo::StringType})
  PARAM_VAR(_deviceNameOut, "OutputDevice",
            "An optional name of the input device to use, otherwise the default input device will be used.",
            {CoreInfo::NoneType, CoreInfo::StringType})

  PARAM_IMPL(PARAM_IMPL_FOR(_inChannels), PARAM_IMPL_FOR(_outChannels), PARAM_IMPL_FOR(_sampleRate), PARAM_IMPL_FOR(_bufferSize),
             PARAM_IMPL_FOR(_deviceNameIn), PARAM_IMPL_FOR(_deviceNameOut));

  void setup() {
    _inChannels = Var(0);
    _outChannels = Var(2);
    _sampleRate = Var(0);
    _bufferSize = Var(0);
  }

  PARAM_REQUIRED_VARIABLES()
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  static const SHTable *properties() { return &experimental.payload.tableValue; }

  ma_context _context;
  mutable ma_device _device;
  mutable bool _open{false};
  bool _started{false};
  SHVar *_deviceVar{nullptr};
  SHVar *_deviceVarDsp{nullptr};

  // (bus, channels hash)
  // don't use those when auto thread is running
  std::unordered_map<uint32_t, std::unordered_map<uint64_t, std::vector<ChannelData *>>> channels;
  std::unordered_map<uint32_t, std::unordered_map<uint64_t, std::vector<float>>> outputBuffers;

  mutable boost::lockfree::queue<ChannelDesc> newChannels{16};

  ma_uint32 actualBufferSize{1024};
  std::vector<float> inputScratch;
  uint64_t inputHash;
  uint64_t outputHash;
  std::shared_ptr<SHMesh> dspMesh = SHMesh::make();
  std::shared_ptr<SHWire> dspWire = SHWire::make("Audio-DSP-Wire");
  SHContext dspContext{nullptr, dspWire.get()};
  std::atomic_bool stopped{false};
  std::atomic_bool hasErrors{false};
  std::string errorMessage;

  SHExposedTypesInfo exposedVariables() {
    static std::array<SHExposedTypeInfo, 1> exposing;
    exposing[0].name = "Audio.Device";
    exposing[0].help = SHCCSTR("The audio device.");
    exposing[0].exposedType = ObjType;
    exposing[0].isProtected = true;
    return {exposing.data(), 1, 0};
  }

  static void pcmCallback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount) {
    auto device = reinterpret_cast<Device *>(pDevice->pUserData);

    shassert(device->_inChannels.payload.intValue == 0 || pDevice->capture.format == ma_format_f32 && "Invalid capture format");
    shassert(device->_outChannels.payload.intValue == 0 ||
             pDevice->playback.format == ma_format_f32 && "Invalid playback format");

    if (device->stopped)
      return;

    device->actualBufferSize = frameCount;

    // add any new channels
    ChannelDesc c;
    while (device->newChannels.pop(c)) {
      // Inside here, if happens, there might be syscalls
      // Allocations and such... we need to refactor if we start
      // Noticing audio cracks
      SHLOG_TRACE("Audio: Adding channel inBus={} inHash={} outBus={} outHash={} outChannels={} inChannels={}", c.inBus, c.inHash,
                  c.outBus, c.outHash, c.outChannels, c.data->inChannels.size());
      {
        auto &bus = device->channels[c.inBus];
        bus[c.inHash].emplace_back(c.data);
      }
      {
        if (c.outChannels > 0) {
          auto &bus = device->outputBuffers[c.outBus];
          auto &buffer = bus[c.outHash];
          buffer.resize(frameCount * c.outChannels);
          c.data->outputBuffer = buffer.data();
          c.data->outputBufferSize = buffer.size();
        } else {
          c.data->outputBuffer = nullptr;
          c.data->outputBufferSize = 0;
        }
      }

      // copy all needed variables
      auto &externalVariables = device->dspWire->getExternalVariables();
      for (auto &[name, var] : c.data->initialVariables) {
        var.flags |= SHVAR_FLAGS_EXTERNAL;
        externalVariables[name] = SHExternalVariable{&var};
      }

      c.data->shards.warmup(&device->dspContext);
    }

    auto inChannels = device->_inChannels.payload.intValue;

    // clear all output buffers as from now we will += to them
    for (auto &[_, buffers] : device->outputBuffers) {
      for (auto &[_, buffer] : buffers) {
        memset(buffer.data(), 0x0, buffer.size() * sizeof(float));
      }
    }

    // batch by output layout
    for (auto &[nbus, channelKinds] : device->channels) {
      for (auto &[kind, channels] : channelKinds) {
        if (channels.size() == 0)
          continue;

        // build the buffer with whatever we need as input
        auto nChannels = SHInt(channels[0]->inChannels.size());
        nChannels = std::min(nChannels, inChannels);

        device->inputScratch.resize(frameCount * nChannels);

        if (nbus == 0) {
          // Device input - deinterleave from miniaudio's interleaved format to planar
          auto *finput = reinterpret_cast<const float *>(pInput);
          if (kind == device->inputHash && inChannels == nChannels) {
            // Full device input, deinterleave all channels
            audioDeinterleave(finput, device->inputScratch.data(), frameCount, uint8_t(nChannels));
          } else {
            // Selective channels - deinterleave only the channels we need
            for (uint32_t c = 0; c < uint32_t(nChannels); c++) {
              float *dst = device->inputScratch.data() + c * frameCount;
              uint32_t srcChannel = channels[0]->inChannels[c];
              for (ma_uint32 i = 0; i < frameCount; i++) {
                dst[i] = finput[i * inChannels + srcChannel];
              }
            }
          }
        } else {
          // Input from another bus - already planar
          const auto inputBuffer = device->outputBuffers[nbus][kind];
          if (inputBuffer.size() != 0) {
            std::copy(inputBuffer.begin(), inputBuffer.end(), device->inputScratch.begin());
          } else {
            memset(device->inputScratch.data(), 0x0, device->inputScratch.size() * sizeof(float));
          }
        }

        SHAudio inputPacket = makeAudio(device->inputScratch.data(), frameCount, uint32_t(device->_sampleRate.payload.intValue),
                                        uint8_t(nChannels));
        Var inputVar(inputPacket);

        // run activations of all channels that need such input
        for (auto channel : channels) {
          SHVar output{};
          device->dspWire->currentInput = inputVar;
          if (channel->shards.activate(&device->dspContext, inputVar, output) == SHWireState::Stop) {
            device->stopped = true;
            // always cleanup or we risk to break someone's ears
            memset(pOutput, 0x0, frameCount * sizeof(float));
            return;
          }
          if (output.valueType == SHType::Audio) {
            if (output.payload.audioValue.nsamples != frameCount) {
              device->errorMessage = "Invalid output audio buffer size";
              // this atomic will be read at the next iteration
              device->hasErrors = true;
              device->stopped = true;
              // always cleanup or we risk to break someone's ears
              memset(pOutput, 0x0, frameCount * sizeof(float));
              return;
            }
            // Only write to output buffer if we have output channels
            if (channel->outChannels.size() > 0) {
              auto &a = output.payload.audioValue;
              for (uint32_t i = 0; i < a.channels * a.nsamples; i++) {
                channel->outputBuffer[i] += a.samples[i] * channel->volume.get().payload.floatValue;
              }
            }
          }
        }
      }
    }

    // finally bake the device buffer with soft clipping
    auto *fOutput = reinterpret_cast<float *>(pOutput);
    auto outChannels = device->_outChannels.payload.intValue;
    auto numSamples = frameCount * outChannels;

    // Clear output buffer first
    memset(pOutput, 0x0, numSamples * sizeof(float));

    // Soft clipping: only engage above ±0.9 threshold to preserve volume at normal levels
    // Below threshold: linear pass-through
    // Above threshold: smooth transition to ±1.0 limit using tanh
    auto softClip = [](float x) -> float {
      constexpr float threshold = 0.9f;
      if (std::abs(x) <= threshold) {
        return x; // Linear region - no volume loss
      } else {
        // Above threshold: smoothly compress to ±1.0
        // Map [0.9..inf] to [0.9..1.0] using tanh
        float sign = x > 0.0f ? 1.0f : -1.0f;
        float excess = (std::abs(x) - threshold) / (1.0f - threshold); // 0..inf
        float compressed = std::tanh(excess);                          // 0..1
        return sign * (threshold + compressed * (1.0f - threshold));
      }
    };

    // Compose device output from all channel outputs on bus 0
    // Channel outputs are planar, device output is interleaved (for miniaudio)
    for (auto &[nbus, channelKinds] : device->channels) {
      if (nbus != 0)
        continue; // Only device output bus
      for (auto &[kind, channels] : channelKinds) {
        if (channels.empty())
          continue;
        auto *channelData = channels[0];
        if (channelData->outChannels.empty())
          continue;

        // Get the output buffer for this channel configuration (planar layout)
        auto &channelOutput = channelData->outputBuffer;
        if (!channelOutput)
          continue;

        auto channelOutCount = channelData->outChannels.size();
        auto requiredBufferSize = channelOutCount * frameCount;
        if (channelData->outputBufferSize < requiredBufferSize) {
          SHLOG_ERROR("Channel output buffer too small: {} < {}", channelData->outputBufferSize, requiredBufferSize);
          continue;
        }

        // Map each channel's planar output to the correct interleaved device output channel
        for (size_t c = 0; c < channelOutCount; c++) {
          auto deviceChannel = channelData->outChannels[c];
          if (deviceChannel >= outChannels)
            continue;

          // Source is planar: channel c data is at channelOutput + c * frameCount
          const float *src = channelOutput + c * frameCount;
          for (ma_uint32 i = 0; i < frameCount; i++) {
            // Add to interleaved device output (mixing)
            fOutput[i * outChannels + deviceChannel] += src[i];
          }
        }
      }
    }

    // Apply soft clipping to final output
    for (size_t i = 0; i < numSamples; i++) {
      fOutput[i] = softClip(fOutput[i]);
    }
  }

  void warmup(SHContext *context) {
    dspWire->mesh = dspMesh;

    _deviceVar = referenceVariable(context, "Audio.Device");
    _deviceVar->valueType = SHType::Object;
    _deviceVar->payload.objectVendorId = CoreCC;
    _deviceVar->payload.objectTypeId = DeviceCC;
    _deviceVar->payload.objectValue = this;

    _deviceVarDsp = referenceVariable(&dspContext, "Audio.Device");
    _deviceVarDsp->valueType = SHType::Object;
    _deviceVarDsp->payload.objectVendorId = CoreCC;
    _deviceVarDsp->payload.objectTypeId = DeviceCC;
    _deviceVarDsp->payload.objectValue = this;

    _context = {};
    if (ma_context_init(NULL, 0, NULL, &_context) != MA_SUCCESS) {
      throw WarmupError("Failed to create ma_context");
    }

    ma_device_id *in_device_id = NULL;
    ma_device_id *out_device_id = NULL;
    if (!_deviceNameIn->isNone() || !_deviceNameOut->isNone()) {
      ma_device_info *pPlaybackInfos;
      ma_uint32 playbackCount;
      ma_device_info *pCaptureInfos;
      ma_uint32 captureCount;
      if (ma_context_get_devices(&_context, &pPlaybackInfos, &playbackCount, &pCaptureInfos, &captureCount) != MA_SUCCESS) {
        throw WarmupError("Failed to enumerate audio devices");
      }

      if (!_deviceNameOut->isNone()) {
        for (ma_uint32 i = 0; i < playbackCount; i++) {
          SHLOG_DEBUG("Output device: {}", pPlaybackInfos[i].name);
          if (strcmp(pPlaybackInfos[i].name, _deviceNameOut->payload.stringValue) == 0) {
            out_device_id = &pPlaybackInfos[i].id;
            break;
          }
        }
        if (out_device_id == NULL) {
          throw WarmupError("Output device not found");
        } else {
          SHLOG_INFO("Output device found: {}", _deviceNameOut);
        }
      }

      if (!_deviceNameIn->isNone()) {
        for (ma_uint32 i = 0; i < captureCount; i++) {
          SHLOG_DEBUG("Input device: {}", pCaptureInfos[i].name);
          if (strcmp(pCaptureInfos[i].name, _deviceNameIn->payload.stringValue) == 0) {
            in_device_id = &pCaptureInfos[i].id;
            break;
          }
        }
        if (in_device_id == NULL) {
          throw WarmupError("Input device not found");
        } else {
          SHLOG_INFO("Input device found: {}", _deviceNameIn);
        }
      }
    }

    ma_device_config deviceConfig{};
    deviceConfig = ma_device_config_init(_inChannels.payload.intValue > 0 ? ma_device_type_duplex : ma_device_type_playback);

    if (_outChannels.payload.intValue > 0) {
      deviceConfig.playback.pDeviceID = out_device_id;
      deviceConfig.playback.format = ma_format_f32;
      deviceConfig.playback.channels = decltype(deviceConfig.playback.channels)(_outChannels.payload.intValue);
      // Use simple channel mixing to avoid semantic remapping (e.g. FRONT_LEFT/RIGHT)
      // This ensures channels map 1:1 by index, not by speaker position
      deviceConfig.playback.channelMixMode = ma_channel_mix_mode_simple;
    }

    if (_inChannels.payload.intValue > 0) {
      deviceConfig.capture.pDeviceID = in_device_id;
      deviceConfig.capture.format = ma_format_f32;
      deviceConfig.capture.channels = decltype(deviceConfig.capture.channels)(_inChannels.payload.intValue);
      deviceConfig.capture.shareMode = ma_share_mode_shared;
      // Use simple channel mixing to avoid semantic remapping (e.g. FRONT_LEFT/RIGHT)
      // This ensures channels map 1:1 by index, not by speaker position
      deviceConfig.capture.channelMixMode = ma_channel_mix_mode_simple;
    }

    deviceConfig.sampleRate = decltype(deviceConfig.sampleRate)(_sampleRate.payload.intValue);
    deviceConfig.periodSizeInFrames = decltype(deviceConfig.periodSizeInFrames)(_bufferSize.payload.intValue);
    deviceConfig.periods = 1;
    deviceConfig.performanceProfile = ma_performance_profile_low_latency;
    deviceConfig.noPreSilencedOutputBuffer = 1; // we do that only if needed
    deviceConfig.dataCallback = pcmCallback;
    deviceConfig.pUserData = this;
#ifdef __APPLE__
    deviceConfig.coreaudio.allowNominalSampleRateChange = 1;
#endif

    if (ma_device_init(&_context, &deviceConfig, &_device) != MA_SUCCESS) {
      throw WarmupError("Failed to open default audio device");
    }

    // fix up the actual sample rate
    _sampleRate = Var(int64_t(_device.sampleRate));

    inputScratch.resize(deviceConfig.periodSizeInFrames * deviceConfig.capture.channels);

    {
      constexpr uint32_t inputSalt = 0x494E5055; // "INPU"
      SHInt inChannels = SHInt(deviceConfig.capture.channels);
      uint32_t bus{0};
      XXH3_state_s hashState;
      XXH3_INITSTATE(&hashState);
      XXH3_64bits_reset_withSecret(&hashState, CUSTOM_XXH3_kSecret, XXH_SECRET_DEFAULT_SIZE);
      XXH3_64bits_update(&hashState, &inputSalt, sizeof(uint32_t));
      XXH3_64bits_update(&hashState, &bus, sizeof(uint32_t));
      for (SHInt i = 0; i < inChannels; i++) {
        XXH3_64bits_update(&hashState, &i, sizeof(SHInt));
      }

      inputHash = XXH3_64bits_digest(&hashState);
    }

    {
      constexpr uint32_t outputSalt = 0x4F555450; // "OUTP"
      SHInt outChannels = SHInt(deviceConfig.playback.channels);
      uint32_t bus{0};
      XXH3_state_s hashState;
      XXH3_INITSTATE(&hashState);
      XXH3_64bits_reset_withSecret(&hashState, CUSTOM_XXH3_kSecret, XXH_SECRET_DEFAULT_SIZE);
      XXH3_64bits_update(&hashState, &outputSalt, sizeof(uint32_t));
      XXH3_64bits_update(&hashState, &bus, sizeof(uint32_t));
      for (SHInt i = 0; i < outChannels; i++) {
        XXH3_64bits_update(&hashState, &i, sizeof(SHInt));
      }

      outputHash = XXH3_64bits_digest(&hashState);
    }

    SHLOG_TRACE("Audio device opened: capture.channels={} playback.channels={} sampleRate={} inputHash={} outputHash={}",
                _device.capture.channels, _device.playback.channels, _device.sampleRate, inputHash, outputHash);

    _open = true;
    stopped = false;
  }

  void stop() const {
    if (_open) {
      ma_device_uninit(&_device);
      _open = false;
    }
  }

  void cleanup(SHContext *context) {
    stop();

    if (_deviceVar) {
      releaseVariable(_deviceVar);
      _deviceVar = nullptr;
    }

    if (_deviceVarDsp) {
      releaseVariable(_deviceVarDsp);
      _deviceVarDsp = nullptr;
    }

    _started = false;
    stopped = false;
    hasErrors = false;
    channels.clear();
  }

  void activate(SHContext *context, const SHVar &input) {
    // refresh this
    _deviceVar->payload.objectValue = this;

    if (!_started) {
      if (ma_device_start(&_device) != MA_SUCCESS) {
        throw ActivationError("Failed to start audio device");
      }
      _started = true;
    }

    if (hasErrors) {
      throw ActivationError(errorMessage);
    }

    if (stopped) {
      context->stopFlow(Var::Empty);
    }
  }
};

struct Channel {
  static SHOptionalString help() {
    return SHCCSTR("This shard represents an audio channel in the mesh. It manages the routing and processing of audio data "
                   "between input and output buses, applies volume control, and executes custom audio processing shards. "
                   "Audio.Channel works in conjunction with Audio.Device to handle audio processing within the shards system.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("Any input type is accepted. The input will be passed to the code specified in the Shards parameter.");
  }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString outputHelp() { return SHCCSTR("Outputs the output of the code specified in the Shards parameter."); }

  static const SHTable *properties() { return &experimental.payload.tableValue; }

  ChannelData _data{};
  SHVar *_device{nullptr};
  const Device *d{nullptr};
  uint32_t _inBusNumber{0};
  OwnedVar _inChannels;
  uint32_t _outBusNumber{0};
  OwnedVar _outChannels;

  void setup() {
    std::array<SHVar, 2> stereo;
    stereo[0] = Var(0);
    stereo[1] = Var(1);
    _inChannels = Var(stereo);
    _outChannels = Var(stereo);
  }

  static inline Parameters Params{
      {"InputBus",
       SHCCSTR("The an integer representing the input bus number. 0 represents the audio device's Analog-to-Digital Converter "
               "(ADC)."),
       {CoreInfo::IntType}},
      {"InputChannels",
       SHCCSTR("A list of input channel indices to be used as input for the code specified in the Shards parameter."),
       {CoreInfo::IntSeqType}},
      {"OutputBus",
       SHCCSTR("The output bus number. 0 represents the audio device's Digital-to-Analog Converter (DAC)."),
       {CoreInfo::IntType}},
      {"OutputChannels",
       SHCCSTR("The list of output channel indices where the processed audio output from the code in the Shards parameter will "
               "be written."),
       {CoreInfo::IntSeqType}},
      {"Volume",
       SHCCSTR("A float value representing the volume level of this channel. Accepts values between 0.0 (mute) and 1.0 (full "
               "volume)."),
       {CoreInfo::FloatType, CoreInfo::FloatVarType}},
      {"Shards", SHCCSTR("The code that will process the audio data."), {CoreInfo::ShardsOrNone}}};

  SHParametersInfo parameters() { return Params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _inBusNumber = uint32_t(value.payload.intValue);
      break;
    case 1:
      _inChannels = value;
      break;
    case 2:
      _outBusNumber = uint32_t(value.payload.intValue);
      break;
    case 3:
      _outChannels = value;
      break;
    case 4:
      _data.volume = value;
      break;
    case 5:
      _data.shards = value;
      break;
    default:
      throw InvalidParameterIndex();
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(int64_t(_inBusNumber));
    case 1:
      return _inChannels;
    case 2:
      return Var(int64_t(_outBusNumber));
    case 3:
      return _outChannels;
    case 4:
      return _data.volume;
    case 5:
      return _data.shards;
    default:
      throw InvalidParameterIndex();
    }
  }

  std::deque<ParamVar> _vars;

  SHTypeInfo compose(const SHInstanceData &data) {
    // Wire needs to capture all it needs, so we need to copy it!
    // this is triggered by populating requiredVariables variable
    auto dataCopy = data;
    dataCopy.requiredVariables = &data.wire->requirements; // this ensures we get the right requirements deep
    dataCopy.inputType = CoreInfo::AudioType;

    _vars.clear();
    auto res = _data.shards.compose(dataCopy);
    for (SHExposedTypeInfo &req : res.requiredInfo) {
      // Capture if not global as we need to copy it!
      SHLOG_TRACE("Audio.Channel: adding variable to requirements: {}", req.name);
      SHVar ctxVar{};
      ctxVar.valueType = SHType::ContextVar;
      ctxVar.payload.stringValue = req.name;
      ctxVar.payload.stringLen = strlen(req.name);
      auto &p = _vars.emplace_back();
      p = ctxVar;
    }

    return data.inputType;
  }

  uint64_t inHash = 0;
  uint64_t outHash = 0;
  uint32_t outChannels = 0;

  void warmup(SHContext *context) {
    _device = referenceVariable(context, "Audio.Device");
    if (_device->valueType == SHType::None) {
      throw ActivationError("Audio.Device not found");
    }

    d = reinterpret_cast<const Device *>(_device->payload.objectValue);

    {
      constexpr uint32_t inputSalt = 0x494E5055; // "INPU"
      XXH3_state_s hashState;
      XXH3_INITSTATE(&hashState);
      XXH3_64bits_reset_withSecret(&hashState, CUSTOM_XXH3_kSecret, XXH_SECRET_DEFAULT_SIZE);
      XXH3_64bits_update(&hashState, &inputSalt, sizeof(uint32_t));
      XXH3_64bits_update(&hashState, &_inBusNumber, sizeof(uint32_t));
      if (_inChannels.valueType == SHType::Seq) {
        for (auto &channel : _inChannels) {
          XXH3_64bits_update(&hashState, &channel.payload.intValue, sizeof(SHInt));
          _data.inChannels.emplace_back(channel.payload.intValue);
        }
      }
      inHash = XXH3_64bits_digest(&hashState);
    }

    {
      constexpr uint32_t outputSalt = 0x4F555450; // "OUTP"
      XXH3_state_s hashState;
      XXH3_INITSTATE(&hashState);
      XXH3_64bits_reset_withSecret(&hashState, CUSTOM_XXH3_kSecret, XXH_SECRET_DEFAULT_SIZE);
      XXH3_64bits_update(&hashState, &outputSalt, sizeof(uint32_t));
      XXH3_64bits_update(&hashState, &_outBusNumber, sizeof(uint32_t));
      if (_outChannels.valueType == SHType::Seq) {
        outChannels = _outChannels.payload.seqValue.len;
        for (auto &channel : _outChannels) {
          XXH3_64bits_update(&hashState, &channel.payload.intValue, sizeof(SHInt));
          _data.outChannels.emplace_back(channel.payload.intValue);
        }
      }
      outHash = XXH3_64bits_digest(&hashState);
    }

    // shards warmup done in audio thread!
    _data.volume.warmup(context);

    for (auto &v : _vars) {
      SHLOG_TRACE("Audio.Channel: warming up variable: {}", v.variableName());
      v.warmup(context);
    }
  }

  void cleanup(SHContext *context) {
    for (auto &v : _vars) {
      v.cleanup();
    }

    if (d) {
      // every device user needs to try and stop it!
      // else we risk to mess with the audio thread
      d->stop();
      d = nullptr;
    }

    if (_device) {
      releaseVariable(_device);
      _device = nullptr;
    }

    _data.shards.cleanup(context);
    _data.volume.cleanup(context);

    _data.initialVariables.clear();

    _started = false;
  }

  bool _started{false};

  void activate(SHContext *context, const SHVar &input) {
    if (!_started) {
      // setup captured variables as mesh externals
      std::deque<shards::OwnedVar> capturedVars;
      for (auto &v : _vars) {
        auto &var = v.get();
        OwnedVar name{Var(v.variableNameView())};
        _data.initialVariables[name] = var;
      }

      ChannelDesc cd{_inBusNumber, inHash, _outBusNumber, outHash, outChannels, &_data};
      d->newChannels.push(cd);

      // _data.shards warmup is done in the audio thread
      _started = true;
    }
  }
  // Must be able to handle device inputs, being an instrument, Aux, busses
  // re-route and send
};

struct ReadFile {
  ma_decoder _decoder;
  bool _initialized{false};

  ma_uint64 _progress{0};

  std::vector<float> _buffer;         // Planar output buffer
  std::vector<float> _interleavedBuf; // Scratch for miniaudio interleaved output
  bool _done{false};

  SHVar *_device{nullptr};
  Device *d{nullptr};

  static SHOptionalString help() {
    return SHCCSTR("This shard reads audio data from a file or memory buffer. It supports various audio formats "
                   "including wav, ogg, mp3, and flac. Audio.ReadFile is designed to be used in conjunction with "
                   "Audio.Device and Audio.Channel to process and play audio in the shards system. It provides "
                   "the audio data that can be further processed or played through the audio device.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHOptionalString inputHelp() { return DefaultHelpText::InputHelpIgnored; }
  static SHTypesInfo outputTypes() { return CoreInfo::AudioType; }
  static SHOptionalString outputHelp() {
    return SHCCSTR("Outputs audio data as an Audio chunk, containing the sample rate, number of samples, "
                   "number of channels, and the audio samples.");
  }

  static const SHTable *properties() { return &experimental.payload.tableValue; }

  void setup() {
    _channels = Var(2);
    _sampleRate = Var(44100);
    _nsamples = Var(1024);
    _looped = Var(false);
  }

  PARAM_PARAMVAR(_source, "Source", "The audio file or bytes to read from (wav,ogg,mp3,flac).",
                 {CoreInfo::StringType, CoreInfo::StringVarType, CoreInfo::BytesType, CoreInfo::BytesVarType});
  PARAM_VAR(_channels, "Channels", "An int representing the number of desired output audio channels.", {CoreInfo::IntType});
  PARAM_VAR(_sampleRate, "SampleRate", "An int representing the desired output sampling rate.", {CoreInfo::IntType});
  PARAM_VAR(_nsamples, "Samples", "An int representing the desired number of samples in the output.", {CoreInfo::IntType});
  PARAM_VAR(_looped, "Looped",
            "A boolean value indicating whether the audio file should be played in loop or should stop the wire when it ends.",
            {CoreInfo::BoolType});
  PARAM_PARAMVAR(_fromSample, "From", "A float value representing the starting time in seconds.",
                 {CoreInfo::FloatType, CoreInfo::FloatVarType, CoreInfo::NoneType});
  PARAM_PARAMVAR(_toSample, "To", "A float value representing the end time in seconds.",
                 {CoreInfo::FloatType, CoreInfo::FloatVarType, CoreInfo::NoneType});

  PARAM_IMPL(PARAM_IMPL_FOR(_source), PARAM_IMPL_FOR(_channels), PARAM_IMPL_FOR(_sampleRate), PARAM_IMPL_FOR(_nsamples),
             PARAM_IMPL_FOR(_looped), PARAM_IMPL_FOR(_fromSample), PARAM_IMPL_FOR(_toSample));

  PARAM_REQUIRED_VARIABLES()
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);

    _device = referenceVariable(context, "Audio.Device");
    if (_device->valueType == SHType::Object) {
      d = reinterpret_cast<Device *>(_device->payload.objectValue);
      // we have a device! override SR and BS
      _sampleRate = d->_sampleRate;
      _nsamples = d->_bufferSize; // this might be less
    }

    _done = false;
    _progress = 0;
  }

  SHVar *previousSource{nullptr};

  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);

    if (_initialized) {
      ma_decoder_uninit(&_decoder);
      memset(&_decoder, 0, sizeof(ma_decoder));
      _initialized = false;
    }

    if (_device) {
      releaseVariable(_device);
      _device = nullptr;
      d = nullptr;
    }

    previousSource = nullptr;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    ma_uint32 channels = ma_uint32(_channels.payload.intValue);
    ma_uint64 nsamples = ma_uint64(_nsamples.payload.intValue);
    ma_uint32 sampleRate = ma_uint32(_sampleRate.payload.intValue);

    auto &source = _source.get();
    if (&source != previousSource) {
      previousSource = &source;

      if (_initialized) {
        ma_decoder_uninit(&_decoder);
        memset(&_decoder, 0, sizeof(ma_decoder));
        _initialized = false;
      }

      ma_decoder_config config = ma_decoder_config_init(ma_format_f32, channels, sampleRate);
      ma_result res;
      if (source.valueType == SHType::String) {
        OwnedVar file = source; // ensure null termination
        res = ma_decoder_init_file(file.payload.stringValue, &config, &_decoder);
      } else if (source.valueType == SHType::Bytes) {
        res = ma_decoder_init_memory(source.payload.bytesValue, source.payload.bytesSize, &config, &_decoder);
      } else {
        throw ActivationError("Invalid audio source type");
      }

      if (res != MA_SUCCESS) {
        SHLOG_ERROR("Failed to open audio source {}", source);
        throw ActivationError("Failed to open audio file");
      }

      ma_uint64 totalSamples;
      ma_decoder_get_length_in_pcm_frames(&_decoder, &totalSamples);

      auto bufSize = size_t(channels) * size_t(nsamples);
      _buffer.resize(bufSize);
      _interleavedBuf.resize(bufSize);
      _initialized = true;
    }

    if (d) {
      // if a device is connected override this value
      nsamples = d->actualBufferSize;
      auto bufSize = size_t(channels) * size_t(nsamples);
      _buffer.resize(bufSize);
      _interleavedBuf.resize(bufSize);
    }

    if (unlikely(_done)) {
      if (_looped.payload.boolValue) {
        ma_result res = ma_decoder_seek_to_pcm_frame(&_decoder, 0);
        if (res != MA_SUCCESS) {
          throw ActivationError("Failed to seek");
        }
        _done = false;
        _progress = 0;
      } else {
        context->stopFlow(Var::Empty);
        return Var::Empty;
      }
    }

    const auto from = _fromSample.get();
    if (unlikely(from.valueType == SHType::Float && _progress == 0)) {
      const auto sfrom = ma_uint64(double(sampleRate) * from.payload.floatValue);
      ma_result res = ma_decoder_seek_to_pcm_frame(&_decoder, sfrom);
      _progress = sfrom;
      if (res != MA_SUCCESS) {
        throw ActivationError("Failed to seek");
      }
    }

    auto reading = nsamples;
    const auto to = _toSample.get();
    if (unlikely(to.valueType == SHType::Float)) {
      const auto sto = ma_uint64(double(sampleRate) * to.payload.floatValue);
      const auto until = _progress + reading;
      if (sto < until) {
        reading = reading - (until - sto);
      }
    }

    // read pcm data every iteration (miniaudio outputs interleaved)
    ma_uint64 framesRead = 0;
    if (reading > 0) {
      ma_result res = ma_decoder_read_pcm_frames(&_decoder, _interleavedBuf.data(), reading, &framesRead);
      if (res != MA_SUCCESS) {
        throw ActivationError("Failed to read");
      }
      _progress += framesRead;
    }

    if (framesRead < nsamples) {
      // Reached the end.
      _done = true;
      // zero anything that was not used in interleaved buffer
      const auto remains = nsamples - framesRead;
      const size_t zeroStart = framesRead * channels;
      const size_t zeroSize = remains * channels;
      if (zeroStart + zeroSize <= _interleavedBuf.size()) {
        memset(_interleavedBuf.data() + zeroStart, 0, sizeof(float) * zeroSize);
      } else {
        // Handle error: buffer is smaller than expected
        throw ActivationError("Buffer size mismatch");
      }
    }

    // Deinterleave to planar output
    audioDeinterleave(_interleavedBuf.data(), _buffer.data(), uint32_t(nsamples), uint8_t(channels));

    return Var(makeAudio(_buffer.data(), uint32_t(nsamples), sampleRate, uint8_t(channels)));
  }
};

struct ReadFileBytes {
  ma_decoder _decoder;
  bool _initialized{false};
  std::vector<float> _buffer;
  ma_uint32 _channels{2};
  ma_uint32 _sampleRate{44100};

  static SHOptionalString help() {
    return SHCCSTR(
        "This shard reads an entire audio file into bytes. Unlike Audio.ReadFile, this shard reads the complete file at once "
        "and outputs it as bytes, without the uint16_t sample count limitation of SHAudio. This is useful when you need to "
        "process large audio files in their entirety.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHOptionalString inputHelp() { return SHCCSTR("The path to the audio file to read."); }
  static SHTypesInfo outputTypes() { return CoreInfo::BytesType; }
  static SHOptionalString outputHelp() { return SHCCSTR("The complete audio file contents as raw PCM float32 samples."); }

  static inline Parameters params{
      {"Channels", SHCCSTR("An int representing the number of desired output audio channels."), {CoreInfo::IntType}},
      {"SampleRate", SHCCSTR("An int representing the desired output sampling rate."), {CoreInfo::IntType}}};

  static SHParametersInfo parameters() { return params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _channels = ma_uint32(value.payload.intValue);
      break;
    case 1:
      _sampleRate = ma_uint32(value.payload.intValue);
      break;
    default:
      throw InvalidParameterIndex();
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_channels);
    case 1:
      return Var(_sampleRate);
    default:
      throw InvalidParameterIndex();
    }
  }

  void cleanup(SHContext *context) {
    if (_initialized) {
      ma_decoder_uninit(&_decoder);
      _initialized = false;
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    OwnedVar file = input; // ensure null termination

    if (_initialized) {
      ma_decoder_uninit(&_decoder);
      _initialized = false;
    }

    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, _channels, _sampleRate);
    ma_result res = ma_decoder_init_file(file.payload.stringValue, &config, &_decoder);
    if (res != MA_SUCCESS) {
      throw ActivationError("Failed to open audio file");
    }
    _initialized = true;

    ma_uint64 totalFrames;
    res = ma_decoder_get_length_in_pcm_frames(&_decoder, &totalFrames);
    if (res != MA_SUCCESS) {
      cleanup(context);
      throw ActivationError("Failed to get audio file length");
    }

    auto channels = _decoder.outputChannels;
    _buffer.resize(totalFrames * channels);

    ma_uint64 framesRead;
    res = ma_decoder_read_pcm_frames(&_decoder, _buffer.data(), totalFrames, &framesRead);
    if (res != MA_SUCCESS) {
      cleanup(context);
      throw ActivationError("Failed to read audio file");
    }

    // Create bytes var with the raw PCM data
    SHVar output;
    output.valueType = SHType::Bytes;
    output.payload.bytesSize = _buffer.size() * sizeof(float);
    output.payload.bytesValue = static_cast<uint8_t *>(malloc(output.payload.bytesSize));
    memcpy(output.payload.bytesValue, _buffer.data(), output.payload.bytesSize);

    cleanup(context);
    return output;
  }
};

struct WriteFile {
  ma_encoder _encoder;
  bool _initialized{false};

  ma_uint32 _channels{2};
  ma_uint32 _sampleRate{44100};
  ma_uint64 _progress{0};
  ParamVar _filename;
  std::vector<float> _interleavedBuf; // Scratch for interleaving planar input

  static SHOptionalString help() { return SHCCSTR("This shard writes audio data to WAV format file."); }

  static SHTypesInfo inputTypes() { return CoreInfo::AudioType; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("Accepts audio data as an Audio chunk, containing the sample rate, number of samples, "
                   "number of channels, and the audio samples.");
  }
  static SHTypesInfo outputTypes() { return CoreInfo::AudioType; }
  static SHOptionalString outputHelp() {
    return SHCCSTR("Outputs the same audio data as the input, allowing for further processing in the audio chain.");
  }

  static const SHTable *properties() { return &experimental.payload.tableValue; }

  static inline Parameters params{
      {"File", SHCCSTR("The audio file to write to (.wav)."), {CoreInfo::StringType, CoreInfo::StringVarType}},
      {"Channels", SHCCSTR("An int representing the number of desired output audio channels."), {CoreInfo::IntType}},
      {"SampleRate", SHCCSTR("An int representing the desired number of samples in the output."), {CoreInfo::IntType}}};

  static SHParametersInfo parameters() { return params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _filename = value;
      break;
    case 1:
      _channels = ma_uint32(value.payload.intValue);
      break;
    case 2:
      _sampleRate = ma_uint32(value.payload.intValue);
      break;
    default:
      throw InvalidParameterIndex();
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _filename;
    case 1:
      return Var(_channels);
    case 2:
      return Var(_sampleRate);
    default:
      throw InvalidParameterIndex();
    }
  }

  void initFile(const std::string_view &filename) {
    ma_encoder_config config = ma_encoder_config_init(ma_encoding_format_wav, ma_format_f32, _channels, _sampleRate);
    ma_result res = ma_encoder_init_file(filename.data(), &config, &_encoder);
    if (res != MA_SUCCESS) {
      SHLOG_ERROR("Failed to open audio encoder on file {}", filename);
      throw ActivationError("Failed to open encoder on file");
    }
  }

  void deinitFile() { ma_encoder_uninit(&_encoder); }

  void warmup(SHContext *context) {
    _filename.warmup(context);

    if (!_filename.isVariable() && _filename->valueType == SHType::String) {
      const auto fname = SHSTRVIEW(_filename.get());
      initFile(fname);
      _initialized = true;
    }

    _progress = 0;
  }

  void cleanup(SHContext *context) {
    _filename.cleanup();

    if (_initialized) {
      deinitFile();
      _initialized = false;
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto &audio = input.payload.audioValue;
    if (audio.channels != _channels) {
      throw ActivationError("Input has an invalid number of audio channels");
    }
    if (!_initialized) {
      const auto fname = SHSTRVIEW(_filename.get());
      initFile(fname);
      _initialized = true;
    }

    // miniaudio encoder expects interleaved format
    // Interleave planar input before writing
    _interleavedBuf.resize(audio.nsamples * audio.channels);
    audioInterleave(audio.samples, _interleavedBuf.data(), audio.nsamples, audio.channels);

    ma_encoder_write_pcm_frames(&_encoder, _interleavedBuf.data(), audio.nsamples, NULL);
    return input;
  }
};

struct Resample {
  ma_resampler _resampler;
  bool _initialized{false};

  static SHOptionalString help() { return SHCCSTR("This shard resamples audio data."); }

  static SHTypesInfo inputTypes() { return CoreInfo::AudioType; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("Accepts audio data as an Audio chunk, containing the sample rate, number of samples, "
                   "number of channels, and the audio samples.");
  }
  static SHTypesInfo outputTypes() { return CoreInfo::AudioType; }
  static SHOptionalString outputHelp() { return SHCCSTR("Outputs the resampled audio data."); }

  static const SHTable *properties() { return &experimental.payload.tableValue; }

  PARAM_PARAMVAR(_outRate, "SampleRate", "The output sample rate.", {CoreInfo::IntType, CoreInfo::IntVarType});

  PARAM_IMPL(PARAM_IMPL_FOR(_outRate));

  PARAM_REQUIRED_VARIABLES()
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  void warmup(SHContext *context) {
    PARAM_WARMUP(context);
    _leftoverSamples.clear();
  }

  void cleanup(SHContext *context) {
    if (_initialized) {
      ma_resampler_uninit(&_resampler, NULL);
      _initialized = false;
    }

    PARAM_CLEANUP(context);
  }

  std::vector<float> _buffer;          // Planar output buffer
  std::vector<float> _interleavedIn;   // Interleaved input for resampler (includes leftovers)
  std::vector<float> _interleavedOut;  // Interleaved output from resampler
  std::vector<float> _leftoverSamples; // Interleaved leftovers
  ma_uint32 _inSampleRate{0};
  ma_uint32 _outSampleRate{0};
  ma_uint32 _channels{0};

  SHVar activate(SHContext *context, const SHVar &input) {
    auto &audio = input.payload.audioValue;
    uint32_t inSampleRate = audioGetSampleRate(audio);

    if (!_initialized) {
      ma_resampler_config config = ma_resampler_config_init(ma_format_f32, audio.channels, inSampleRate,
                                                            _outRate.get().payload.intValue, ma_resample_algorithm_linear);
      ma_result res = ma_resampler_init(&config, NULL, &_resampler);
      if (res != MA_SUCCESS) {
        throw ActivationError("Failed to initialize resampler");
      }
      _initialized = true;
      _inSampleRate = inSampleRate;
      _outSampleRate = _outRate.get().payload.intValue;
      _channels = audio.channels;
    }

    if (inSampleRate != _inSampleRate) {
      throw ActivationError("Input sample rate does not match initialized sample rate");
    }

    // Build interleaved input: leftovers + new samples (single buffer, no intermediate copy)
    const size_t newSamplesInterleaved = audio.nsamples * _channels;
    const size_t leftoverSamplesInterleaved = _leftoverSamples.size();
    const size_t totalSamplesInterleaved = leftoverSamplesInterleaved + newSamplesInterleaved;

    _interleavedIn.resize(totalSamplesInterleaved);

    // Copy leftovers to front (if any)
    if (leftoverSamplesInterleaved > 0) {
      memcpy(_interleavedIn.data(), _leftoverSamples.data(), leftoverSamplesInterleaved * sizeof(float));
    }

    // Interleave new planar samples directly after leftovers
    audioInterleave(audio.samples, _interleavedIn.data() + leftoverSamplesInterleaved, audio.nsamples, uint8_t(_channels));

    ma_uint64 totalFramesIn = totalSamplesInterleaved / _channels;
    ma_uint64 frameCountIn = totalFramesIn;
    ma_uint64 frameCountOut = 0;

    // Get expected output frame count
    ma_result res = ma_resampler_get_expected_output_frame_count(&_resampler, frameCountIn, &frameCountOut);
    if (res != MA_SUCCESS) {
      throw ActivationError("Failed to get expected output frame count");
    }

    _interleavedOut.resize(frameCountOut * _channels);
    res = ma_resampler_process_pcm_frames(&_resampler, _interleavedIn.data(), &frameCountIn, _interleavedOut.data(),
                                          &frameCountOut);

    if (res != MA_SUCCESS) {
      SHLOG_ERROR("Failed to resample audio: {} {}", res, frameCountIn);
      throw ActivationError("Failed to resample audio");
    }

    // Store unconsumed samples for next iteration (in interleaved format)
    if (frameCountIn < totalFramesIn) {
      size_t consumedSamples = frameCountIn * _channels;
      _leftoverSamples.assign(_interleavedIn.begin() + consumedSamples, _interleavedIn.end());
    } else {
      _leftoverSamples.clear();
    }

    // Convert interleaved output back to planar
    _buffer.resize(frameCountOut * _channels);
    audioDeinterleave(_interleavedOut.data(), _buffer.data(), uint32_t(frameCountOut), uint8_t(_channels));

    return Var(makeAudio(_buffer.data(), uint32_t(frameCountOut), _outSampleRate, uint8_t(_channels)));
  }
};

struct Engine {
  static inline shards::logging::Logger Logger = shards::logging::getOrCreate("audio");
  static constexpr uint32_t EngineCC = 'snde';

  static inline Type ObjType{{SHType::Object, {.object = {.vendorId = CoreCC, .typeId = EngineCC}}}};

  std::unique_ptr<ma_log> _log;

  ma_engine _engine;
  bool _initialized{false};

  static SHOptionalString help() {
    return SHCCSTR(
        "This shard initializes an audio engine in the mesh, and this enables audio playback and processing capabilites. "
        "It manages resources, handles audio mixing, and provides spatial audio "
        "functionality. The Audio.Engine is used in conjunction with other audio shards like Audio.Sound, Audio.Play "
        "and Audio.Pause to create a complete audio system to process and play audio.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString inputHelp() { return DefaultHelpText::InputHelpPass; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static SHOptionalString outputHelp() { return DefaultHelpText::OutputHelpPass; }

  SHVar *_deviceVar{nullptr};

  static void ma_log_cb(void *pUserData, ma_uint32 level, const char *pMessage) {
    std::string_view msg(pMessage);
    // trim newline from string view
    if (!msg.empty() && msg.back() == '\n') {
      msg.remove_suffix(1);
    }
    switch (level) {
    case MA_LOG_LEVEL_DEBUG:
      SPDLOG_LOGGER_DEBUG(Engine::Logger, "{}", msg);
      break;
    case MA_LOG_LEVEL_INFO:
      SPDLOG_LOGGER_DEBUG(Engine::Logger, "{}", msg);
      break;
    case MA_LOG_LEVEL_WARNING:
      SPDLOG_LOGGER_WARN(Engine::Logger, "{}", msg);
      break;
    case MA_LOG_LEVEL_ERROR:
      SPDLOG_LOGGER_ERROR(Engine::Logger, "{}", msg);
      break;
    }
  }

  void warmup(SHContext *context) {
    _log = std::make_unique<::ma_log>();
    ma_log_init(nullptr, _log.get());
    ma_log_register_callback(_log.get(), ma_log_callback_init(&ma_log_cb, this));

    ma_result res{};
    proxyToMainThread([&]() {
      ma_engine_config config = ma_engine_config_init();
      config.pLog = _log.get();
      res = ma_engine_init(&config, &_engine);
    });
    if (res != MA_SUCCESS) {
      throw ActivationError("Failed to init audio engine");
    }

    _deviceVar = referenceVariable(context, "Audio.Engine");
    _deviceVar->valueType = SHType::Object;
    _deviceVar->payload.objectVendorId = CoreCC;
    _deviceVar->payload.objectTypeId = EngineCC;
    _deviceVar->payload.objectValue = &_engine;

    _initialized = true;
  }

  void cleanup(SHContext *context) {
    if (_log) {
      ma_log_uninit(_log.get());
      _log.reset();
    }
    if (_initialized) {
      proxyToMainThread([&]() { ma_engine_uninit(&_engine); });
      memset(&_engine, 0, sizeof(ma_engine));
      releaseVariable(_deviceVar);
      _initialized = false;
    }
  }

  SHExposedTypesInfo exposedVariables() {
    static std::array<SHExposedTypeInfo, 1> exposing;
    exposing[0].name = "Audio.Engine";
    exposing[0].help = SHCCSTR("The audio engine.");
    exposing[0].exposedType = ObjType;
    exposing[0].isProtected = true;
    return {exposing.data(), 1, 0};
  }

  SHVar activate(SHContext *context, const SHVar &input) { return input; }
};

struct EngineUser {
  ma_engine *_engine{nullptr};
  SHVar *_engineVar{nullptr};

  void warmup(SHContext *context) {
    _engineVar = referenceVariable(context, "Audio.Engine");
    if (_engineVar->valueType == SHType::Object) {
      _engine = reinterpret_cast<ma_engine *>(_engineVar->payload.objectValue);
    } else {
      throw ActivationError("Audio.Engine not found");
    }
  }

  void cleanup(SHContext *context) {
    if (_engineVar) {
      _engine = nullptr;
      releaseVariable(_engineVar);
      _engineVar = nullptr;
    }
  }
};

struct Sound : EngineUser {
  static constexpr uint32_t SoundCC = 'snds';

  static inline Type ObjType{{SHType::Object, {.object = {.vendorId = CoreCC, .typeId = SoundCC}}}};
  static inline Type SoundVarType = Type::VariableOf(ObjType);

  static SHOptionalString help() {
    return SHCCSTR(
        "The Audio.Sound shard initializes a sound object in the mesh. It loads an audio file and prepares it for playback. "
        "This shard is used in conjunction with other audio shards like Audio.Start, Audio.Pause, and "
        "Audio.Stop to control audio playback. It supports spatialization for 3D audio positioning and can be used "
        "with various audio effect shards for further processing. Do note that the Spatialized parameter on Audio.Sound should "
        "be set to true when initializing a sound object meant for 3D audio (if it is to be manipulated by Audio.Direction, "
        "Audio.Position, Audio.Velocity or Audio.Cones).");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("Accepts a string representing the path to the audio file or asset to be loaded.");
  }
  static SHTypesInfo outputTypes() { return ObjType; }
  static SHOptionalString outputHelp() { return SHCCSTR("Outputs a Sound object that can be used with other audio shards."); }

  PARAM_VAR(_spatialized, "Spatialized", "If the sound should have 3D audio capabilities.", {CoreInfo::BoolType});
  PARAM_IMPL(PARAM_IMPL_FOR(_spatialized));

  void setup() { _spatialized = Var(false); }

  std::optional<ma_sound> _sound;
  OwnedVar _fileName;

  void warmup(SHContext *context) { EngineUser::warmup(context); }

  void cleanup(SHContext *context) {
    EngineUser::cleanup(context);

    if (_sound) {
      ma_sound_uninit(&*_sound);
      _sound.reset();
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    _fileName = input; // ensure null termination

    if (_sound) {
      ma_sound_uninit(&*_sound);
      _sound.reset();
    }

    _sound.emplace();
    ma_uint32 flags = MA_SOUND_FLAG_DECODE | MA_SOUND_FLAG_ASYNC;
    if (!_spatialized.payload.boolValue) {
      flags |= MA_SOUND_FLAG_NO_SPATIALIZATION;
    }
    ma_result result{};
    proxyToMainThread([&]() {
      result = result = ma_sound_init_from_file(_engine, _fileName.payload.stringValue, flags, NULL, NULL, &*_sound);
    });
    if (result != MA_SUCCESS) {
      throw ActivationError("Failed to init sound");
    }

    return Var::Object(&*_sound, CoreCC, SoundCC);
  }
};

struct Start : EngineUser {
  static SHOptionalString help() {
    return SHCCSTR(
        "The Audio.Start shard begins playback of a sound object in the mesh. It takes a Sound object "
        "created by Audio.Sound and starts playing it and also allows control over whether the sound "
        "should loop or play once. It's typically used in conjunction with Audio.Engine, Audio.Sound, Audio.Pause, and "
        "Audio.Stop to manage audio playback.");
  }

  static SHTypesInfo inputTypes() { return Sound::ObjType; }
  static SHOptionalString inputHelp() { return SHCCSTR("Accepts a Sound object created by the Audio.Sound shard."); }
  static SHTypesInfo outputTypes() { return Sound::ObjType; }
  static SHOptionalString outputHelp() {
    return SHCCSTR("Outputs the same Sound object that was input, allowing for further manipulation.");
  }

  void setup() { _looped = Var(false); }

  // Looped parameter
  PARAM_VAR(_looped, "Looped", "If the sound should be played in loop or should stop the wire when it ends and play only once.",
            {CoreInfo::BoolType});
  PARAM_IMPL(PARAM_IMPL_FOR(_looped));

  PARAM_REQUIRED_VARIABLES()
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    return outputTypes().elements[0];
  }

  void warmup(SHContext *context) {
    EngineUser::warmup(context);
    PARAM_WARMUP(context);
  }

  void cleanup(SHContext *context) {
    PARAM_CLEANUP(context);
    EngineUser::cleanup(context);
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto _sound = reinterpret_cast<ma_sound *>(input.payload.objectValue);

    proxyToMainThread([&]() {
      ma_sound_set_looping(&*_sound, _looped.payload.boolValue);
      ma_sound_start(&*_sound);
    });
    return input;
  }
};

struct Pause {
  static SHOptionalString help() {
    return SHCCSTR("The Audio.Pause shard pauses playback of a sound object in the mesh. It takes a Sound object "
                   "which was created by Audio.Sound and played by Audio.Start and pauses its playback. This shard is typically "
                   "used in conjunction with "
                   "Audio.Engine, Audio.Sound, Audio.Start, and Audio.Stop to manage audio playback and control.");
  }
  static SHTypesInfo inputTypes() { return Sound::ObjType; }
  static SHOptionalString inputHelp() { return SHCCSTR("Accepts a Sound object created by the Audio.Sound shard."); }
  static SHTypesInfo outputTypes() { return Sound::ObjType; }
  static SHOptionalString outputHelp() {
    return SHCCSTR("Outputs the same Sound object that was input, allowing for further manipulation.");
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto _sound = reinterpret_cast<ma_sound *>(input.payload.objectValue);

    proxyToMainThread([&]() { ma_sound_stop(&*_sound); });
    return input;
  }
};

struct Stop {
  static SHOptionalString help() {
    return SHCCSTR("The Audio.Stop shard stops playback of a sound object in the mesh. It takes a Sound object "
                   "which was created by Audio.Sound and played with Audio.Start and stops its playback, resetting the playback "
                   "position to the beginning. "
                   "This shard is typically used in conjunction with Audio.Engine, Audio.Sound, Audio.Start, and "
                   "Audio.Pause to manage audio playback and control.");
  }

  static SHTypesInfo inputTypes() { return Sound::ObjType; }
  static SHOptionalString inputHelp() { return SHCCSTR("Accepts a Sound object created by the Audio.Sound shard."); }
  static SHTypesInfo outputTypes() { return Sound::ObjType; }
  static SHOptionalString outputHelp() {
    return SHCCSTR("Outputs the same Sound object that was input, allowing for further manipulation.");
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto _sound = reinterpret_cast<ma_sound *>(input.payload.objectValue);

    proxyToMainThread([&]() {
      ma_sound_stop(&*_sound);
      // and reset timeline to 0
      ma_sound_seek_to_pcm_frame(&*_sound, 0);
    });
    return input;
  }
};

struct SetVolume {
  static SHOptionalString help() {
    return SHCCSTR(
        "The Audio.Volume shard adjusts the volume of a sound object in the mesh, thus allowing for the dynamic control over the "
        "volume of individual sound objects during playback. It takes the Sound object, created by Audio.Sound "
        "specified in the Sound parameter, and sets the volume to the float value provided as input. "
        "It's typically used "
        "in conjunction with Audio.Engine, Audio.Sound, Audio.Start, and other audio shards to manage "
        "audio playback and control.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::FloatType; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("A float value representing the new volume level. 0.0 is silence, 1.0 is full volume, "
                   "and values above 1.0 can be used for amplification.");
  }
  static SHTypesInfo outputTypes() { return CoreInfo::FloatType; }
  static SHOptionalString outputHelp() { return DefaultHelpText::OutputHelpPass; }

  PARAM_PARAMVAR(_sound, "Sound", AudioDefaultHelpText::SoundObjectParam, {Sound::ObjType, Sound::SoundVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_sound));

  PARAM_REQUIRED_VARIABLES()
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    if (_sound.isNone()) {
      throw ActivationError("Sound is required");
    }
    return outputTypes().elements[0];
  }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto sound = reinterpret_cast<ma_sound *>(_sound.get().payload.objectValue);
    ma_sound_set_volume(&*sound, input.payload.floatValue);
    return input;
  }
};

struct SetPan {
  static SHOptionalString help() {
    return SHCCSTR(
        "The Audio.Pan shard adjusts the stereo panning of a sound object in the mesh, allowing for dynamic control over "
        "the spatial positioning of individual sound objects during playback. It takes the Sound object, created by Audio.Sound "
        "that is specified in the Sound parameter, and sets the pan position to the float value provided as input (-1.0 being "
        "full left and 1.0 being full right). "
        "It's typically used in conjunction with Audio.Engine, Audio.Sound, Audio.Start, and other audio shards to manage "
        "audio playback and create spatial audio effects.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::FloatType; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("A float value representing the new pan position. -1.0 is full left, 0.0 is center, and 1.0 is full right. "
                   "Values outside of this range will be clamped to the nearest extreme.");
  }
  static SHTypesInfo outputTypes() { return CoreInfo::FloatType; }
  static SHOptionalString outputHelp() { return DefaultHelpText::OutputHelpPass; }

  PARAM_PARAMVAR(_sound, "Sound", AudioDefaultHelpText::SoundObjectParam, {Sound::ObjType, Sound::SoundVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_sound));

  PARAM_REQUIRED_VARIABLES()
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    if (_sound.isNone()) {
      throw ActivationError("Sound is required");
    }
    return outputTypes().elements[0];
  }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto sound = reinterpret_cast<ma_sound *>(_sound.get().payload.objectValue);
    ma_sound_set_pan(&*sound, input.payload.floatValue);
    return input;
  }
};

struct SetPitch {
  static SHOptionalString help() {
    return SHCCSTR(
        "The Audio.Pitch shard adjusts the pitch of a sound object in the mesh, thus allowing for dynamic control over the pitch "
        "of individual sound objects during playback. It takes the Sound object, created by Audio.Sound "
        "that is specified in the Sound parameter, and sets the pitch to the float value provided as input. 1.0 being the "
        "original pitch, "
        "values greater than 1.0 will increase the pitch, while values between 0 and 1.0 will decrease the pitch. 0.5, for "
        "example, will lower the pitch by one octave, "
        "while 2.0 will raise it by one octave. "
        "It's typically used in conjunction with "
        "Audio.Engine, Audio.Sound, Audio.Start, and other audio shards to manage audio playback and create pitch-based "
        "effects.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::FloatType; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("A float value representing the new pitch. 1.0 being the original pitch.");
  }
  static SHTypesInfo outputTypes() { return CoreInfo::FloatType; }
  static SHOptionalString outputHelp() { return DefaultHelpText::OutputHelpPass; }

  PARAM_PARAMVAR(_sound, "Sound", AudioDefaultHelpText::SoundObjectParam, {Sound::ObjType, Sound::SoundVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_sound));

  PARAM_REQUIRED_VARIABLES()
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    if (_sound.isNone()) {
      throw ActivationError("Sound is required");
    }
    return outputTypes().elements[0];
  }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto sound = reinterpret_cast<ma_sound *>(_sound.get().payload.objectValue);
    ma_sound_set_pitch(&*sound, input.payload.floatValue);
    return input;
  }
};

struct SetPosition {
  static SHOptionalString help() {
    return SHCCSTR(
        "The Audio.Position shard sets the 3D position of a sound object in the audio space. It takes the Sound object, "
        "created by Audio.Sound, that is specified in the Sound parameter, and sets its position to the 3D coordinates "
        "represented as a float3 vector(a vector with 3 float elements) provided as input. This shard is particularly useful for "
        "creating spatial audio effects and is typically used "
        "in conjunction with Audio.Engine, Audio.Sound, and Audio.Direction to manage 3D audio positioning and orientation. Do "
        "note that the Spatialized parameter on Audio.Sound should be set to true when initializing a sound object meant for 3D "
        "audio.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::Float3Type; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("A float3 vector representing the new 3D position (x, y, z coordinates) of the sound.");
  }
  static SHTypesInfo outputTypes() { return CoreInfo::FloatType; }
  static SHOptionalString outputHelp() { return DefaultHelpText::OutputHelpPass; }

  PARAM_PARAMVAR(_sound, "Sound", AudioDefaultHelpText::SoundObjectParam, {Sound::ObjType, Sound::SoundVarType});

  PARAM_IMPL(PARAM_IMPL_FOR(_sound));

  PARAM_REQUIRED_VARIABLES()
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    if (_sound.isNone()) {
      throw ActivationError("Sound is required");
    }
    return outputTypes().elements[0];
  }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto sound = reinterpret_cast<ma_sound *>(_sound.get().payload.objectValue);
    ma_sound_set_position(&*sound, input.payload.float3Value[0], input.payload.float3Value[1], input.payload.float3Value[2]);
    return input;
  }
};

struct SetDirection {
  static SHOptionalString help() {
    return SHCCSTR("The Audio.Direction shard sets the direction of a sound object in 3D audio space. It takes the Sound object, "
                   "created by Audio.Sound that is specified in the Sound parameter, and sets its direction to the 3D vector "
                   "with x y z coordinates, represented as a float3 vector(a vector with 3 float elements), "
                   "that is provided as input. The x coordinate represents its direction along the x-axis, the y coordinate "
                   "represents its direction along the y-axis, and the z coordinate represents its direction along the z-axis. "
                   "The float3 vector input should also be normalized so that it has a magnitude of 1. This shard is "
                   "particularly useful for creating directional audio effects in 3D environments "
                   "and is typically used in conjunction with Audio.Engine, Audio.Sound, and Audio.Position to manage 3D audio "
                   "positioning and orientation. Do note that the Spatialized parameter on Audio.Sound should be set to true "
                   "when initializing a sound object meant for 3D audio.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::Float3Type; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("A float3 vector representing the new direction (x, y, z components) of the sound. "
                   "This vector should be normalized (have a magnitude of 1). ");
  }
  static SHTypesInfo outputTypes() { return CoreInfo::Float3Type; }
  static SHOptionalString outputHelp() { return DefaultHelpText::OutputHelpPass; }

  PARAM_PARAMVAR(_sound, "Sound", AudioDefaultHelpText::SoundObjectParam, {Sound::ObjType, Sound::SoundVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_sound));

  PARAM_REQUIRED_VARIABLES()
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    if (_sound.isNone()) {
      throw ActivationError("Sound is required");
    }
    return outputTypes().elements[0];
  }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto sound = reinterpret_cast<ma_sound *>(_sound.get().payload.objectValue);
    ma_sound_set_direction(&*sound, input.payload.float3Value[0], input.payload.float3Value[1], input.payload.float3Value[2]);
    return input;
  }
};

struct SetCones {
  static SHOptionalString help() {
    return SHCCSTR(
        "The Audio.Cones shard sets the sound cone properties for a 3D sound object. Sound cones are used to create directional "
        "audio effects, where the volume of the sound changes based on the angle between the sound's direction and the "
        "listener's position. "
        "It takes the Sound object, created by Audio.Sound that is specified in the Sound parameter, and sets its cone "
        "properties using "
        "the float3 vector(a vector with 3 float elements) provided as input. The first float value in the float3 vector "
        "represents the inner angle in radians, the second float value represents the outer angle in radians, and the third "
        "float value represents the outer gain. "
        "This shard is particularly useful for creating directional audio effects in 3D environments and is typically used in "
        "conjunction with Audio.Engine, Audio.Sound, Audio.Position, and Audio.Direction. Do note that the Spatialized parameter "
        "on Audio.Sound should be set to true when initializing a sound object meant for 3D audio.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::Float3Type; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("A float3 vector with each element representing the respective cone properties: "
                   "innerAngleInRadians (the angle within which the sound is at full volume), "
                   "outerAngleInRadians (the angle at which the sound starts to attenuate), "
                   "and outerGain (the volume multiplier for sounds outside the outer angle).");
  }

  static SHTypesInfo outputTypes() { return CoreInfo::Float3Type; }
  static SHOptionalString outputHelp() { return DefaultHelpText::OutputHelpPass; }

  PARAM_PARAMVAR(_sound, "Sound", AudioDefaultHelpText::SoundObjectParam, {Sound::ObjType, Sound::SoundVarType});
  PARAM_IMPL(PARAM_IMPL_FOR(_sound));

  PARAM_REQUIRED_VARIABLES()
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    if (_sound.isNone()) {
      throw ActivationError("Sound is required");
    }
    return outputTypes().elements[0];
  }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto sound = reinterpret_cast<ma_sound *>(_sound.get().payload.objectValue);
    ma_sound_set_cone(&*sound, input.payload.float2Value[0], input.payload.float3Value[1], input.payload.float3Value[2]);
    return input;
  }
};

struct SetVelocity {
  static SHOptionalString help() {
    return SHCCSTR(
        "The Audio.Velocity shard sets the velocity of a 3D sound object in the audio space. It takes the Sound object, "
        "created by Audio.Sound that is specified in the Sound parameter, and sets its velocity to the 3D vector, represented as "
        "a float3 vector(a vector with 3 float elements), "
        "provided as input. The first element in the float3 vector represents the velocity along the x-axis, the second element "
        "represents the velocity along the y-axis, and the third element represents the velocity along the z-axis. "
        "This shard is particularly useful for creating doppler effects and is typically used "
        "in conjunction with Audio.Engine, Audio.Sound, Audio.Position, and Audio.Direction to manage 3D audio positioning and "
        "movement. Do note that the Spatialized parameter on Audio.Sound should be set to true when initializing a sound object "
        "meant for 3D audio.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::Float3Type; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("A float3 vector representing the new velocity (each float element representing the velocity along the x, y, "
                   "and z axes respectively) of the sound in units per second.");
  }
  static SHTypesInfo outputTypes() { return CoreInfo::Float3Type; }
  static SHOptionalString outputHelp() { return DefaultHelpText::OutputHelpPass; }

  PARAM_PARAMVAR(_sound, "Sound", AudioDefaultHelpText::SoundObjectParam, {Sound::ObjType, Sound::SoundVarType});

  PARAM_IMPL(PARAM_IMPL_FOR(_sound));

  PARAM_REQUIRED_VARIABLES()
  SHTypeInfo compose(SHInstanceData &data) {
    PARAM_COMPOSE_REQUIRED_VARIABLES(data);
    if (_sound.isNone()) {
      throw ActivationError("Sound is required");
    }
    return outputTypes().elements[0];
  }

  void warmup(SHContext *context) { PARAM_WARMUP(context); }

  void cleanup(SHContext *context) { PARAM_CLEANUP(context); }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto sound = reinterpret_cast<ma_sound *>(_sound.get().payload.objectValue);
    ma_sound_set_velocity(&*sound, input.payload.float3Value[0], input.payload.float3Value[1], input.payload.float3Value[2]);
    return input;
  }
};

void registerCompressorShards();
void registerCodecShards();

// Accessor functions for Device - used by synth.cpp
uint32_t getDeviceBufferSize(const Device *device) { return device->actualBufferSize; }

uint32_t getDeviceSampleRate(const Device *device) { return uint32_t(device->_sampleRate.payload.intValue); }

} // namespace Audio
} // namespace shards

SHARDS_REGISTER_FN(audio) {
  using namespace shards::Audio;
  REGISTER_SHARD("Audio.Device", shards::Audio::Device);
  REGISTER_SHARD("Audio.Channel", shards::Audio::Channel);
  REGISTER_SHARD("Audio.ReadFile", shards::Audio::ReadFile);
  REGISTER_SHARD("Audio.ReadFileBytes", shards::Audio::ReadFileBytes);
  REGISTER_SHARD("Audio.WriteFile", shards::Audio::WriteFile);
  REGISTER_SHARD("Audio.Resample", shards::Audio::Resample);

  REGISTER_SHARD("Audio.Engine", shards::Audio::Engine);

  REGISTER_SHARD("Audio.Sound", shards::Audio::Sound);
  REGISTER_SHARD("Audio.Start", shards::Audio::Start);
  REGISTER_SHARD("Audio.Pause", shards::Audio::Pause);
  REGISTER_SHARD("Audio.Stop", shards::Audio::Stop);
  REGISTER_SHARD("Audio.Volume", shards::Audio::SetVolume);
  REGISTER_SHARD("Audio.Pan", shards::Audio::SetPan);
  REGISTER_SHARD("Audio.Pitch", shards::Audio::SetPitch);
  REGISTER_SHARD("Audio.Position", shards::Audio::SetPosition);
  REGISTER_SHARD("Audio.Direction", shards::Audio::SetDirection);
  REGISTER_SHARD("Audio.Cones", shards::Audio::SetCones);
  REGISTER_SHARD("Audio.Velocity", shards::Audio::SetVelocity);

  shards::registerObjectType(shards::CoreCC, shards::Audio::Device::DeviceCC, SHObjectInfo{"Device"});
  shards::registerObjectType(shards::CoreCC, shards::Audio::Engine::EngineCC, SHObjectInfo{"Engine"});
  shards::registerObjectType(shards::CoreCC, shards::Audio::Sound::SoundCC, SHObjectInfo{"Sound"});

  registerCompressorShards();
  registerCodecShards();
}
