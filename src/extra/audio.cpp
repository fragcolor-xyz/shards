/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

#include "shards/shared.hpp"
#include "runtime.hpp"
#include <boost/lockfree/queue.hpp>

#define STB_VORBIS_HEADER_ONLY
#include "extras/stb_vorbis.c" // Enables Vorbis decoding.

#ifdef __APPLE__
#define MA_NO_RUNTIME_LINKING
#endif

// #ifndef NDEBUG
// #define MA_LOG_LEVEL MA_LOG_LEVEL_WARNING
// #define MA_DEBUG_OUTPUT 1
// #endif
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"

// The stb_vorbis implementation must come after the implementation of
// miniaudio.
#undef STB_VORBIS_HEADER_ONLY
#include "extras/stb_vorbis.c"

namespace shards {
namespace Audio {
static TableVar experimental{{"experimental", Var(true)}};

/*

Inner audio wires should not be allowed to have (Pause) or clipping would
happen Also they should probably run like RunWire Detached so that multiple
references to the same wire would be possible and they would just produce
another iteration

*/

struct ChannelData {
  float *outputBuffer;
  std::vector<uint32_t> inChannels;
  std::vector<uint32_t> outChannels;
  ShardsVar shards;
  ParamVar volume{Var(0.7)};
};

struct ChannelDesc {
  uint32_t inBus;
  uint64_t inHash;
  uint32_t outBus;
  uint64_t outHash;
  uint32_t outChannels;
  ChannelData *data;
};

struct Device {
  static constexpr uint32_t DeviceCC = 'sndd';

  static inline Type ObjType{{SHType::Object, {.object = {.vendorId = CoreCC, .typeId = DeviceCC}}}};

  // TODO add shards used as insert for the final mix

  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

  static const SHTable *properties() { return &experimental.payload.tableValue; }

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

  // we don't want to use this inside our operation callback
  // miniaudio does not follow the same value on certain platforms
  // it's basically a max
  ma_uint32 bufferSize{1024};
  ma_uint32 actualBufferSize{1024};
  ma_uint32 sampleRate{44100};
  ma_uint32 inChannels{2};
  ma_uint32 outChannels{2};
  std::vector<float> inputScratch;
  uint64_t inputHash;
  uint64_t outputHash;
  SHFlow dpsFlow{};
  SHCoro dspStubCoro{};
  std::shared_ptr<SHMesh> dspMesh = SHMesh::make();
  std::shared_ptr<SHWire> dspWire = SHWire::make("Audio-DSP-Wire");
#ifndef __EMSCRIPTEN__
  SHContext dspContext{std::move(dspStubCoro), dspWire.get(), &dpsFlow};
#else
  SHContext dspContext{&dspStubCoro, dspWire.get(), &dpsFlow};
#endif
  std::atomic_bool stopped{false};
  std::atomic_bool hasErrors{false};
  std::string errorMessage;

  static void pcmCallback(ma_device *pDevice, void *pOutput, const void *pInput, ma_uint32 frameCount) {
    assert(pDevice->capture.format == ma_format_f32);
    assert(pDevice->playback.format == ma_format_f32);

    auto device = reinterpret_cast<Device *>(pDevice->pUserData);

    if (device->stopped)
      return;

    // add any new channels
    ChannelDesc c;
    while (device->newChannels.pop(c)) {
      // Inside here, if happens, there might be syscalls
      // Allocations and such... we need to refactor if we start
      // Noticing audio cracks
      {
        auto &bus = device->channels[c.inBus];
        bus[c.inHash].emplace_back(c.data);
      }
      {
        auto &bus = device->outputBuffers[c.outBus];
        auto &buffer = bus[c.outHash];
        buffer.resize(frameCount * c.outChannels);
        c.data->outputBuffer = buffer.data();
      }
      c.data->shards.warmup(&device->dspContext);
    }

    device->actualBufferSize = frameCount;

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
        const auto nchannels = channels[0]->inChannels.size();

        device->inputScratch.resize(frameCount * nchannels);

        if (nbus == 0) {
          if (kind == device->inputHash) {
            // this is the full device input, just copy it
            memcpy(device->inputScratch.data(), pInput, sizeof(float) * nchannels * frameCount);
          } else {
            auto *finput = reinterpret_cast<const float *>(pInput);
            // need to properly compose the input
            for (uint32_t c = 0; c < nchannels; c++) {
              for (ma_uint32 i = 0; i < frameCount; i++) {
                device->inputScratch[(i * nchannels) + c] = finput[(i * nchannels) + channels[0]->inChannels[c]];
              }
            }
          }
        } else {
          const auto inputBuffer = device->outputBuffers[nbus][kind];
          if (inputBuffer.size() != 0) {
            std::copy(inputBuffer.begin(), inputBuffer.end(), device->inputScratch.begin());
          } else {
            memset(device->inputScratch.data(), 0x0, device->inputScratch.size() * sizeof(float));
          }
        }

        SHAudio inputPacket{uint32_t(device->sampleRate), //
                            uint16_t(frameCount),         //
                            uint16_t(nchannels),          //
                            device->inputScratch.data()};
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
            auto &a = output.payload.audioValue;
            for (uint32_t i = 0; i < a.channels * a.nsamples; i++) {
              channel->outputBuffer[i] += a.samples[i] * channel->volume.get().payload.floatValue;
            }
          }
        }
      }
    }

    // finally bake the device buffer
    auto &output = device->outputBuffers[0][device->outputHash];
    if (output.size() > 0) {
      memcpy(pOutput, output.data(), frameCount * sizeof(float) * device->outChannels);
    } else {
      // always cleanup or we risk to break someone's ears
      memset(pOutput, 0x0, frameCount * sizeof(float) * device->outChannels);
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

    ma_device_config deviceConfig{};
    deviceConfig = ma_device_config_init(ma_device_type_duplex);
    deviceConfig.playback.pDeviceID = NULL;
    deviceConfig.playback.format = ma_format_f32;
    deviceConfig.playback.channels = outChannels;
    deviceConfig.capture.pDeviceID = NULL;
    deviceConfig.capture.format = ma_format_f32;
    deviceConfig.capture.channels = inChannels;
    deviceConfig.capture.shareMode = ma_share_mode_shared;
    deviceConfig.sampleRate = sampleRate;
    deviceConfig.periodSizeInFrames = bufferSize;
    deviceConfig.periods = 1;
    deviceConfig.performanceProfile = ma_performance_profile_low_latency;
    deviceConfig.noPreZeroedOutputBuffer = 1; // we do that only if needed
    deviceConfig.dataCallback = pcmCallback;
    deviceConfig.pUserData = this;
#ifdef __APPLE__
    deviceConfig.coreaudio.allowNominalSampleRateChange = 1;
#endif

    if (ma_device_init(NULL, &deviceConfig, &_device) != MA_SUCCESS) {
      throw WarmupError("Failed to open default audio device");
    }

    inputScratch.resize(bufferSize * deviceConfig.capture.channels);

    {
      uint64_t inChannels = uint64_t(deviceConfig.capture.channels);
      uint32_t bus{0};
      XXH3_state_s hashState;
      XXH3_INITSTATE(&hashState);
      XXH3_64bits_reset_withSecret(&hashState, CUSTOM_XXH3_kSecret, XXH_SECRET_DEFAULT_SIZE);
      XXH3_64bits_update(&hashState, &bus, sizeof(uint32_t));
      for (SHInt i = 0; i < SHInt(inChannels); i++) {
        XXH3_64bits_update(&hashState, &i, sizeof(SHInt));
      }

      inputHash = XXH3_64bits_digest(&hashState);
    }

    {
      uint64_t outChannels = uint64_t(deviceConfig.playback.channels);
      uint32_t bus{0};
      XXH3_state_s hashState;
      XXH3_INITSTATE(&hashState);
      XXH3_64bits_reset_withSecret(&hashState, CUSTOM_XXH3_kSecret, XXH_SECRET_DEFAULT_SIZE);
      XXH3_64bits_update(&hashState, &bus, sizeof(uint32_t));
      for (SHInt i = 0; i < SHInt(outChannels); i++) {
        XXH3_64bits_update(&hashState, &i, sizeof(SHInt));
      }

      outputHash = XXH3_64bits_digest(&hashState);
    }

    _open = true;
    stopped = false;
  }

  void stop() const {
    if (_open) {
      ma_device_uninit(&_device);
      _open = false;
    }
  }

  void cleanup() {
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

  SHVar activate(SHContext *context, const SHVar &input) {
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
      return Var::Empty;
    }

    return input;
  }
};

struct Channel {
  static SHTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnyType; }

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
      {"InputBus", SHCCSTR("The input bus number, 0 is the audio device ADC."), {CoreInfo::IntType}},
      {"InputChannels", SHCCSTR("The list of input channels to pass as input to Shards."), {CoreInfo::IntSeqType}},
      {"OutputBus", SHCCSTR("The output bus number, 0 is the audio device DAC."), {CoreInfo::IntType}},
      {"OutputChannels", SHCCSTR("The list of output channels to write from Shards's output."), {CoreInfo::IntSeqType}},
      {"Volume", SHCCSTR("The volume of this channel."), {CoreInfo::FloatType, CoreInfo::FloatVarType}},
      {"Shards", SHCCSTR("The shards that will process audio data."), {CoreInfo::ShardsOrNone}}};

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

  SHTypeInfo compose(const SHInstanceData &data) {
    _data.shards.compose(data);
    return data.inputType;
  }

  void warmup(SHContext *context) {
    _device = referenceVariable(context, "Audio.Device");
    d = reinterpret_cast<const Device *>(_device->payload.objectValue);
    uint64_t inHash = 0;
    uint64_t outHash = 0;
    uint32_t outChannels = 0;
    {
      XXH3_state_s hashState;
      XXH3_INITSTATE(&hashState);
      XXH3_64bits_reset_withSecret(&hashState, CUSTOM_XXH3_kSecret, XXH_SECRET_DEFAULT_SIZE);
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
      XXH3_state_s hashState;
      XXH3_INITSTATE(&hashState);
      XXH3_64bits_reset_withSecret(&hashState, CUSTOM_XXH3_kSecret, XXH_SECRET_DEFAULT_SIZE);
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

    ChannelDesc cd{_inBusNumber, inHash, _outBusNumber, outHash, outChannels, &_data};
    d->newChannels.push(cd);

    // shards warmup done in audio thread!
    _data.volume.warmup(context);
  }

  void cleanup() {
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

    _data.shards.cleanup();
    _data.volume.cleanup();
  }

  SHVar activate(SHContext *context, const SHVar &input) { return input; }
  // Must be able to handle device inputs, being an instrument, Aux, busses
  // re-route and send
};

struct Oscillator {
  enum class Waveform { Sine, Square, Triangle, Sawtooth };
  static constexpr uint32_t WaveformCC = 'wave';
  static inline EnumInfo<Waveform> WaveformEnumInfo{"Waveform", CoreCC, WaveformCC};
  static inline Type WaveformType = Type::Enum(CoreCC, WaveformCC);

  ma_waveform _wave;

  ma_uint32 _channels{2};
  ma_uint64 _nsamples{1024};
  ma_uint32 _sampleRate{44100};

  std::vector<float> _buffer;

  SHVar *_device{nullptr};
  Device *d{nullptr};

  ParamVar _amplitude{Var(0.4)};
  Waveform _type{Waveform::Sine};

  static SHTypesInfo inputTypes() { return CoreInfo::FloatType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AudioType; }

  static const SHTable *properties() { return &experimental.payload.tableValue; }

  static inline Parameters params{
      {"Type", SHCCSTR("The waveform type to oscillate."), {WaveformType}},
      {"Amplitude", SHCCSTR("The waveform amplitude."), {CoreInfo::FloatType, CoreInfo::FloatVarType}},
      {"Channels", SHCCSTR("The number of desired output audio channels."), {CoreInfo::IntType}},
      {"SampleRate",
       SHCCSTR("The desired output sampling rate. Ignored if inside an "
               "Audio.Channel."),
       {CoreInfo::IntType}},
      {"Samples",
       SHCCSTR("The desired number of samples in the output. Ignored if inside "
               "an Audio.Channel."),
       {CoreInfo::IntType}}};

  static SHParametersInfo parameters() { return params; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _type = Waveform(value.payload.enumValue);
      break;
    case 1:
      _amplitude = value;
      break;
    case 2:
      _channels = ma_uint32(value.payload.intValue);
      break;
    case 3:
      _sampleRate = ma_uint32(value.payload.intValue);
      break;
    case 4:
      _nsamples = ma_uint64(value.payload.intValue);
      break;
    default:
      throw InvalidParameterIndex();
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return Var::Enum(_type, CoreCC, WaveformCC);
    case 1:
      return _amplitude;
    case 2:
      return Var(_channels);
    case 3:
      return Var(_sampleRate);
    case 4:
      return Var(int64_t(_nsamples));
    default:
      throw InvalidParameterIndex();
    }
  }

  void initWave() {
    ma_waveform_type wtype;
    switch (_type) {
    case Waveform::Sine:
      wtype = ma_waveform_type_sine;
      break;
    case Waveform::Square:
      wtype = ma_waveform_type_square;
      break;
    case Waveform::Triangle:
      wtype = ma_waveform_type_triangle;
      break;
    case Waveform::Sawtooth:
      wtype = ma_waveform_type_sawtooth;
      break;
    }
    ma_waveform_config config = ma_waveform_config_init(ma_format_f32, _channels, _sampleRate, wtype, 0.0, 1.0);
    ma_result res = ma_waveform_init(&config, &_wave);
    if (res != MA_SUCCESS) {
      throw ActivationError("Failed to init waveform");
    }
  }

  void warmup(SHContext *context) {
    _amplitude.warmup(context);

    _device = referenceVariable(context, "Audio.Device");
    if (_device->valueType == SHType::Object) {
      d = reinterpret_cast<Device *>(_device->payload.objectValue);
      // we have a device! override SR and BS
      _sampleRate = d->sampleRate;
      _nsamples = d->bufferSize; // this might be less
    }

    initWave();
    _buffer.resize(_channels * _nsamples);
  }

  void cleanup() {
    _amplitude.cleanup();

    if (_device) {
      releaseVariable(_device);
      _device = nullptr;
      d = nullptr;
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    if (d) {
      // if a device is connected override this value
      _nsamples = d->actualBufferSize;
    }

    ma_waveform_set_amplitude(&_wave, _amplitude.get().payload.floatValue);
    ma_waveform_set_frequency(&_wave, input.payload.floatValue);

    ma_waveform_read_pcm_frames(&_wave, _buffer.data(), _nsamples);

    return Var(SHAudio{_sampleRate, uint16_t(_nsamples), uint16_t(_channels), _buffer.data()});
  }
};

struct ReadFile {
  ma_decoder _decoder;
  bool _initialized{false};

  ma_uint32 _channels{2};
  ma_uint64 _nsamples{1024};
  ma_uint32 _sampleRate{44100};
  ma_uint64 _progress{0};
  // what to do when not looped ends? throw?
  bool _looped{false};
  ParamVar _fromSample;
  ParamVar _toSample;
  ParamVar _filename;

  std::vector<float> _buffer;
  bool _done{false};

  SHVar *_device{nullptr};
  Device *d{nullptr};

  static SHTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AudioType; }

  static const SHTable *properties() { return &experimental.payload.tableValue; }

  static inline Parameters params{
      {"File", SHCCSTR("The audio file to read from (wav,ogg,mp3,flac)."), {CoreInfo::StringType, CoreInfo::StringVarType}},
      {"Channels", SHCCSTR("The number of desired output audio channels."), {CoreInfo::IntType}},
      {"SampleRate",
       SHCCSTR("The desired output sampling rate. Ignored if inside an "
               "Audio.Channel."),
       {CoreInfo::IntType}},
      {"Samples",
       SHCCSTR("The desired number of samples in the output. Ignored if inside "
               "an Audio.Channel."),
       {CoreInfo::IntType}},
      {"Looped",
       SHCCSTR("If the file should be played in loop or should stop the wire "
               "when it ends."),
       {CoreInfo::BoolType}},
      {"From", SHCCSTR("The starting time in seconds."), {CoreInfo::FloatType, CoreInfo::FloatVarType, CoreInfo::NoneType}},
      {"To", SHCCSTR("The end time in seconds."), {CoreInfo::FloatType, CoreInfo::FloatVarType, CoreInfo::NoneType}}};

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
    case 3:
      _nsamples = ma_uint64(value.payload.intValue);
      break;
    case 4:
      _looped = value.payload.boolValue;
      break;
    case 5:
      _fromSample = value;
      break;
    case 6:
      _toSample = value;
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
    case 3:
      return Var(int64_t(_nsamples));
    case 4:
      return Var(_looped);
    case 5:
      return _fromSample;
    case 6:
      return _toSample;
    default:
      throw InvalidParameterIndex();
    }
  }

  void initFile(const std::string_view &filename) {
    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, _channels, _sampleRate);
    ma_result res = ma_decoder_init_file(filename.data(), &config, &_decoder);
    if (res != MA_SUCCESS) {
      SHLOG_ERROR("Failed to open audio file {}", filename);
      throw ActivationError("Failed to open audio file");
    }
  }

  void deinitFile() { ma_decoder_uninit(&_decoder); }

  void warmup(SHContext *context) {
    _device = referenceVariable(context, "Audio.Device");
    if (_device->valueType == SHType::Object) {
      d = reinterpret_cast<Device *>(_device->payload.objectValue);
      // we have a device! override SR and BS
      _sampleRate = d->sampleRate;
      _nsamples = d->bufferSize; // this might be less
    }

    _fromSample.warmup(context);
    _toSample.warmup(context);
    _filename.warmup(context);

    if (!_filename.isVariable() && _filename->valueType == SHType::String) {
      const auto fname = SHSTRVIEW(_filename.get());
      initFile(fname);
      _buffer.resize(size_t(_channels) * size_t(_nsamples));
      _initialized = true;
    }

    _done = false;
    _progress = 0;
  }

  void cleanup() {
    _fromSample.cleanup();
    _toSample.cleanup();
    _filename.cleanup();

    if (_initialized) {
      deinitFile();
      _initialized = false;
    }

    if (_device) {
      releaseVariable(_device);
      _device = nullptr;
      d = nullptr;
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    if (d) {
      // if a device is connected override this value
      _nsamples = d->actualBufferSize;
    }

    if (unlikely(_done)) {
      if (_looped) {
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
      const auto sfrom = ma_uint64(double(_sampleRate) * from.payload.floatValue);
      ma_result res = ma_decoder_seek_to_pcm_frame(&_decoder, sfrom);
      _progress = sfrom;
      if (res != MA_SUCCESS) {
        throw ActivationError("Failed to seek");
      }
    }

    auto reading = _nsamples;
    const auto to = _toSample.get();
    if (unlikely(to.valueType == SHType::Float)) {
      const auto sto = ma_uint64(double(_sampleRate) * to.payload.floatValue);
      const auto until = _progress + reading;
      if (sto < until) {
        reading = reading - (until - sto);
      }
    }

    // read pcm data every iteration
    ma_uint64 framesRead = ma_decoder_read_pcm_frames(&_decoder, _buffer.data(), reading);
    _progress += framesRead;
    if (framesRead < _nsamples) {
      // Reached the end.
      _done = true;
      // zero anything that was not used
      const auto remains = _nsamples - framesRead;
      memset(_buffer.data() + framesRead, 0x0, sizeof(float) * remains);
    }

    return Var(SHAudio{_sampleRate, uint16_t(_nsamples), uint16_t(_channels), _buffer.data()});
  }
};

struct WriteFile {
  ma_encoder _encoder;
  bool _initialized{false};

  ma_uint32 _channels{2};
  ma_uint32 _sampleRate{44100};
  ma_uint64 _progress{0};
  ParamVar _filename;

  static SHTypesInfo inputTypes() { return CoreInfo::AudioType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AudioType; }

  static const SHTable *properties() { return &experimental.payload.tableValue; }

  static inline Parameters params{
      {"File", SHCCSTR("The audio file to read from (wav,ogg,mp3,flac)."), {CoreInfo::StringType, CoreInfo::StringVarType}},
      {"Channels", SHCCSTR("The number of desired output audio channels."), {CoreInfo::IntType}},
      {"SampleRate", SHCCSTR("The desired output sampling rate."), {CoreInfo::IntType}}};

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
    ma_encoder_config config = ma_encoder_config_init(ma_resource_format_wav, ma_format_f32, _channels, _sampleRate);
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

  void cleanup() {
    _filename.cleanup();

    if (_initialized) {
      deinitFile();
      _initialized = false;
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    if (input.payload.audioValue.channels != _channels) {
      throw ActivationError("Input has an invalid number of audio channels");
    }
    ma_encoder_write_pcm_frames(&_encoder, input.payload.audioValue.samples, input.payload.audioValue.nsamples);
    return input;
  }
};

void registerShards() {
  REGISTER_SHARD("Audio.Device", Device);
  REGISTER_SHARD("Audio.Channel", Channel);
  REGISTER_SHARD("Audio.Oscillator", Oscillator);
  REGISTER_SHARD("Audio.ReadFile", ReadFile);
  REGISTER_SHARD("Audio.WriteFile", WriteFile);
}
} // namespace Audio
} // namespace shards
