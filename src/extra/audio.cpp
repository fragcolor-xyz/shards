/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2021 Giovanni Petrantoni */

#include "blocks/shared.hpp"
#include "runtime.hpp"

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

namespace chainblocks {
namespace Audio {

/*

Inner audio chains should not be allowed to have (Pause) or clipping would
happen Also they should probably run like RunChain Detached so that multiple
references to the same chain would be possible and they would just produce
another iteration

*/

struct ChannelData {
  float *outputBuffer;
  std::vector<uint32_t> inChannels;
  std::vector<uint32_t> outChannels;
  BlocksVar blocks;
  ParamVar volume{Var(0.7)};
};

struct Device {
  static constexpr uint32_t DeviceCC = 'sndd';

  static inline Type ObjType{
      {CBType::Object, {.object = {.vendorId = CoreCC, .typeId = DeviceCC}}}};

  // TODO add blocks used as insert for the final mix

  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  ma_device _device;
  bool _open{false};
  bool _started{false};
  CBVar *_deviceVar{nullptr};
  CBVar *_deviceVarDsp{nullptr};

  // (bus, channels hash)
  mutable std::unordered_map<
      uint32_t, std::unordered_map<uint64_t, std::vector<ChannelData *>>>
      channels;
  mutable std::unordered_map<uint32_t,
                             std::unordered_map<uint64_t, std::vector<float>>>
      outputBuffers;
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
  CBFlow dpsFlow{};
  CBCoro dspStubCoro{};
  std::shared_ptr<CBNode> dspNode = CBNode::make();
  std::shared_ptr<CBChain> dspChain = CBChain::make("Audio-DSP-Chain");
#ifndef __EMSCRIPTEN__
  CBContext dspContext{std::move(dspStubCoro), dspChain.get(), &dpsFlow};
#else
  CBContext dspContext{&dspStubCoro, dspChain.get(), &dpsFlow};
#endif
  std::atomic_bool stopped{false};
  std::atomic_bool hasErrors{false};
  std::string errorMessage;

  static void pcmCallback(ma_device *pDevice, void *pOutput, const void *pInput,
                          ma_uint32 frameCount) {
    assert(pDevice->capture.format == ma_format_f32);
    assert(pDevice->playback.format == ma_format_f32);

    auto device = reinterpret_cast<Device *>(pDevice->pUserData);

    if (device->stopped)
      return;

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
            memcpy(device->inputScratch.data(), pInput,
                   sizeof(float) * nchannels * frameCount);
          } else {
            auto *finput = reinterpret_cast<const float *>(pInput);
            // need to properly compose the input
            for (uint32_t c = 0; c < nchannels; c++) {
              for (ma_uint32 i = 0; i < frameCount; i++) {
                device->inputScratch[(i * nchannels) + c] =
                    finput[(i * nchannels) + channels[0]->inChannels[c]];
              }
            }
          }
        } else {
          const auto inputBuffer = device->outputBuffers[nbus][kind];
          if (inputBuffer.size() != 0) {
            std::copy(inputBuffer.begin(), inputBuffer.end(),
                      device->inputScratch.begin());
          } else {
            memset(device->inputScratch.data(), 0x0,
                   device->inputScratch.size() * sizeof(float));
          }
        }

        CBAudio inputPacket{uint32_t(device->sampleRate), //
                            uint16_t(frameCount),         //
                            uint16_t(nchannels),          //
                            device->inputScratch.data()};
        Var inputVar(inputPacket);

        // run activations of all channels that need such input
        for (auto channel : channels) {
          CBVar output{};
          device->dspChain->currentInput = inputVar;
          if (channel->blocks.activate(&device->dspContext, inputVar, output,
                                       false) == CBChainState::Stop) {
            device->stopped = true;
            // always cleanup or we risk to break someone's ears
            memset(pOutput, 0x0, frameCount * sizeof(float));
            return;
          }
          if (output.valueType == CBType::Audio) {
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
              channel->outputBuffer[i] +=
                  a.samples[i] * channel->volume.get().payload.floatValue;
            }
          }
        }
      }
    }

    // finally bake the device buffer
    auto &output = device->outputBuffers[0][device->outputHash];
    if (output.size() > 0) {
      memcpy(pOutput, output.data(),
             frameCount * sizeof(float) * device->outChannels);
    } else {
      // always cleanup or we risk to break someone's ears
      memset(pOutput, 0x0, frameCount * sizeof(float) * device->outChannels);
    }
  }

  void warmup(CBContext *context) {
    dspChain->node = dspNode;

    _deviceVar = referenceVariable(context, "Audio.Device");
    _deviceVar->valueType = CBType::Object;
    _deviceVar->payload.objectVendorId = CoreCC;
    _deviceVar->payload.objectTypeId = DeviceCC;
    _deviceVar->payload.objectValue = this;

    _deviceVarDsp = referenceVariable(&dspContext, "Audio.Device");
    _deviceVarDsp->valueType = CBType::Object;
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
      XXH3_64bits_reset_withSecret(&hashState, CUSTOM_XXH3_kSecret,
                                   XXH_SECRET_DEFAULT_SIZE);
      XXH3_64bits_update(&hashState, &bus, sizeof(uint32_t));
      for (CBInt i = 0; i < CBInt(inChannels); i++) {
        XXH3_64bits_update(&hashState, &i, sizeof(CBInt));
      }

      inputHash = XXH3_64bits_digest(&hashState);
    }

    {
      uint64_t outChannels = uint64_t(deviceConfig.playback.channels);
      uint32_t bus{0};
      XXH3_state_s hashState;
      XXH3_INITSTATE(&hashState);
      XXH3_64bits_reset_withSecret(&hashState, CUSTOM_XXH3_kSecret,
                                   XXH_SECRET_DEFAULT_SIZE);
      XXH3_64bits_update(&hashState, &bus, sizeof(uint32_t));
      for (CBInt i = 0; i < CBInt(outChannels); i++) {
        XXH3_64bits_update(&hashState, &i, sizeof(CBInt));
      }

      outputHash = XXH3_64bits_digest(&hashState);
    }

    _open = true;
    stopped = false;
  }

  void stop() {
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

  CBVar activate(CBContext *context, const CBVar &input) {
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
      CB_STOP();
    }

    return input;
  }
};

struct Channel {
  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }

  ChannelData _data{};
  CBVar *_device{nullptr};
  Device *d{nullptr};
  uint32_t _inBusNumber{0};
  OwnedVar _inChannels;
  uint32_t _outBusNumber{0};
  OwnedVar _outChannels;

  void setup() {
    std::array<CBVar, 2> stereo;
    stereo[0] = Var(0);
    stereo[1] = Var(1);
    _inChannels = Var(stereo);
    _outChannels = Var(stereo);
  }

  static inline Parameters Params{
      {"InputBus",
       CBCCSTR("The input bus number, 0 is the audio device ADC."),
       {CoreInfo::IntType}},
      {"InputChannels",
       CBCCSTR("The list of input channels to pass as input to Blocks."),
       {CoreInfo::IntSeqType}},
      {"OutputBus",
       CBCCSTR("The output bus number, 0 is the audio device DAC."),
       {CoreInfo::IntType}},
      {"OutputChannels",
       CBCCSTR("The list of output channels to write from Blocks's output."),
       {CoreInfo::IntSeqType}},
      {"Volume",
       CBCCSTR("The volume of this channel."),
       {CoreInfo::FloatType, CoreInfo::FloatVarType}},
      {"Blocks",
       CBCCSTR("The blocks that will process audio data."),
       {CoreInfo::BlocksOrNone}}};

  CBParametersInfo parameters() { return Params; }

  void setParam(int index, const CBVar &value) {
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
      _data.blocks = value;
      break;
    default:
      throw InvalidParameterIndex();
    }
  }

  CBVar getParam(int index) {
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
      return _data.blocks;
    default:
      throw InvalidParameterIndex();
    }
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    _data.blocks.compose(data);
    return data.inputType;
  }

  void warmup(CBContext *context) {
    _device = referenceVariable(context, "Audio.Device");
    d = reinterpret_cast<Device *>(_device->payload.objectValue);
    {
      XXH3_state_s hashState;
      XXH3_INITSTATE(&hashState);
      XXH3_64bits_reset_withSecret(&hashState, CUSTOM_XXH3_kSecret,
                                   XXH_SECRET_DEFAULT_SIZE);
      XXH3_64bits_update(&hashState, &_inBusNumber, sizeof(uint32_t));
      if (_inChannels.valueType == CBType::Seq) {
        for (auto &channel : _inChannels) {
          XXH3_64bits_update(&hashState, &channel.payload.intValue,
                             sizeof(CBInt));
          _data.inChannels.emplace_back(channel.payload.intValue);
        }
      }
      const auto channelsHash = XXH3_64bits_digest(&hashState);
      auto &bus = d->channels[_inBusNumber];
      bus[channelsHash].emplace_back(&_data);
    }
    {
      XXH3_state_s hashState;
      XXH3_INITSTATE(&hashState);
      XXH3_64bits_reset_withSecret(&hashState, CUSTOM_XXH3_kSecret,
                                   XXH_SECRET_DEFAULT_SIZE);
      XXH3_64bits_update(&hashState, &_outBusNumber, sizeof(uint32_t));
      uint32_t nchannels = 0;
      if (_outChannels.valueType == CBType::Seq) {
        nchannels = _outChannels.payload.seqValue.len;
        for (auto &channel : _outChannels) {
          XXH3_64bits_update(&hashState, &channel.payload.intValue,
                             sizeof(CBInt));
          _data.outChannels.emplace_back(channel.payload.intValue);
        }
      }
      const auto channelsHash = XXH3_64bits_digest(&hashState);
      auto &bus = d->outputBuffers[_outBusNumber];
      auto &buffer = bus[channelsHash];
      buffer.resize(d->bufferSize * nchannels);
      _data.outputBuffer = buffer.data();
    }

    _data.blocks.warmup(&d->dspContext);
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

    _data.blocks.cleanup();
    _data.volume.cleanup();
  }

  CBVar activate(CBContext *context, const CBVar &input) { return input; }
  // Must be able to handle device inputs, being an instrument, Aux, busses
  // re-route and send
};

struct Oscillator {
  enum class Waveform { Sine, Square, Triangle, Sawtooth };
  static constexpr uint32_t WaveformCC = 'wave';
  static inline EnumInfo<Waveform> WaveformEnumInfo{"Waveform", CoreCC,
                                                    WaveformCC};
  static inline Type WaveformType = Type::Enum(CoreCC, WaveformCC);

  ma_waveform _wave;

  ma_uint32 _channels{2};
  ma_uint64 _nsamples{1024};
  ma_uint32 _sampleRate{44100};

  std::vector<float> _buffer;

  CBVar *_device{nullptr};
  Device *d{nullptr};

  ParamVar _amplitude{Var(0.4)};
  Waveform _type{Waveform::Sine};

  static CBTypesInfo inputTypes() { return CoreInfo::FloatType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AudioType; }

  static inline Parameters params{
      {"Type", CBCCSTR("The waveform type to oscillate."), {WaveformType}},
      {"Amplitude",
       CBCCSTR("The waveform amplitude."),
       {CoreInfo::FloatType, CoreInfo::FloatVarType}},
      {"Channels",
       CBCCSTR("The number of desired output audio channels."),
       {CoreInfo::IntType}},
      {"SampleRate",
       CBCCSTR("The desired output sampling rate. Ignored if inside an "
               "Audio.Channel."),
       {CoreInfo::IntType}},
      {"Samples",
       CBCCSTR("The desired number of samples in the output. Ignored if inside "
               "an Audio.Channel."),
       {CoreInfo::IntType}}};

  static CBParametersInfo parameters() { return params; }

  void setParam(int index, const CBVar &value) {
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

  CBVar getParam(int index) {
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
    ma_waveform_config config = ma_waveform_config_init(
        ma_format_f32, _channels, _sampleRate, wtype, 0.0, 1.0);
    ma_result res = ma_waveform_init(&config, &_wave);
    if (res != MA_SUCCESS) {
      throw ActivationError("Failed to init waveform");
    }
  }

  void warmup(CBContext *context) {
    _amplitude.warmup(context);

    _device = referenceVariable(context, "Audio.Device");
    if (_device->valueType == CBType::Object) {
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

  CBVar activate(CBContext *context, const CBVar &input) {
    if (d) {
      // if a device is connected override this value
      _nsamples = d->actualBufferSize;
    }

    ma_waveform_set_amplitude(&_wave, _amplitude.get().payload.floatValue);
    ma_waveform_set_frequency(&_wave, input.payload.floatValue);

    ma_waveform_read_pcm_frames(&_wave, _buffer.data(), _nsamples);

    return Var(CBAudio{_sampleRate, uint16_t(_nsamples), uint16_t(_channels),
                       _buffer.data()});
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

  CBVar *_device{nullptr};
  Device *d{nullptr};

  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AudioType; }

  static inline Parameters params{
      {"File",
       CBCCSTR("The audio file to read from (wav,ogg,mp3,flac)."),
       {CoreInfo::StringType, CoreInfo::StringVarType}},
      {"Channels",
       CBCCSTR("The number of desired output audio channels."),
       {CoreInfo::IntType}},
      {"SampleRate",
       CBCCSTR("The desired output sampling rate. Ignored if inside an "
               "Audio.Channel."),
       {CoreInfo::IntType}},
      {"Samples",
       CBCCSTR("The desired number of samples in the output. Ignored if inside "
               "an Audio.Channel."),
       {CoreInfo::IntType}},
      {"Looped",
       CBCCSTR("If the file should be played in loop or should stop the chain "
               "when it ends."),
       {CoreInfo::BoolType}},
      {"From",
       CBCCSTR("The starting time in seconds."),
       {CoreInfo::FloatType, CoreInfo::FloatVarType, CoreInfo::NoneType}},
      {"To",
       CBCCSTR("The end time in seconds."),
       {CoreInfo::FloatType, CoreInfo::FloatVarType, CoreInfo::NoneType}}};

  static CBParametersInfo parameters() { return params; }

  void setParam(int index, const CBVar &value) {
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

  CBVar getParam(int index) {
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
    ma_decoder_config config =
        ma_decoder_config_init(ma_format_f32, _channels, _sampleRate);
    ma_result res = ma_decoder_init_file(filename.data(), &config, &_decoder);
    if (res != MA_SUCCESS) {
      CBLOG_ERROR("Failed to open audio file {}", filename);
      throw ActivationError("Failed to open audio file");
    }
  }

  void deinitFile() { ma_decoder_uninit(&_decoder); }

  void warmup(CBContext *context) {
    _device = referenceVariable(context, "Audio.Device");
    if (_device->valueType == CBType::Object) {
      d = reinterpret_cast<Device *>(_device->payload.objectValue);
      // we have a device! override SR and BS
      _sampleRate = d->sampleRate;
      _nsamples = d->bufferSize; // this might be less
    }

    _fromSample.warmup(context);
    _toSample.warmup(context);
    _filename.warmup(context);

    if (!_filename.isVariable() && _filename->valueType == CBType::String) {
      const auto fname = CBSTRVIEW(_filename.get());
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

  CBVar activate(CBContext *context, const CBVar &input) {
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
        CB_STOP();
      }
    }

    const auto from = _fromSample.get();
    if (unlikely(from.valueType == CBType::Float && _progress == 0)) {
      const auto sfrom =
          ma_uint64(double(_sampleRate) * from.payload.floatValue);
      ma_result res = ma_decoder_seek_to_pcm_frame(&_decoder, sfrom);
      _progress = sfrom;
      if (res != MA_SUCCESS) {
        throw ActivationError("Failed to seek");
      }
    }

    auto reading = _nsamples;
    const auto to = _toSample.get();
    if (unlikely(to.valueType == CBType::Float)) {
      const auto sto = ma_uint64(double(_sampleRate) * to.payload.floatValue);
      const auto until = _progress + reading;
      if (sto < until) {
        reading = reading - (until - sto);
      }
    }

    // read pcm data every iteration
    ma_uint64 framesRead =
        ma_decoder_read_pcm_frames(&_decoder, _buffer.data(), reading);
    _progress += framesRead;
    if (framesRead < _nsamples) {
      // Reached the end.
      _done = true;
      // zero anything that was not used
      const auto remains = _nsamples - framesRead;
      memset(_buffer.data() + framesRead, 0x0, sizeof(float) * remains);
    }

    return Var(CBAudio{_sampleRate, uint16_t(_nsamples), uint16_t(_channels),
                       _buffer.data()});
  }
};

struct WriteFile {
  ma_encoder _encoder;
  bool _initialized{false};

  ma_uint32 _channels{2};
  ma_uint32 _sampleRate{44100};
  ma_uint64 _progress{0};
  ParamVar _filename;

  static CBTypesInfo inputTypes() { return CoreInfo::AudioType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AudioType; }

  static inline Parameters params{
      {"File",
       CBCCSTR("The audio file to read from (wav,ogg,mp3,flac)."),
       {CoreInfo::StringType, CoreInfo::StringVarType}},
      {"Channels",
       CBCCSTR("The number of desired output audio channels."),
       {CoreInfo::IntType}},
      {"SampleRate",
       CBCCSTR("The desired output sampling rate."),
       {CoreInfo::IntType}}};

  static CBParametersInfo parameters() { return params; }

  void setParam(int index, const CBVar &value) {
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

  CBVar getParam(int index) {
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
    ma_encoder_config config = ma_encoder_config_init(
        ma_resource_format_wav, ma_format_f32, _channels, _sampleRate);
    ma_result res = ma_encoder_init_file(filename.data(), &config, &_encoder);
    if (res != MA_SUCCESS) {
      CBLOG_ERROR("Failed to open audio encoder on file {}", filename);
      throw ActivationError("Failed to open encoder on file");
    }
  }

  void deinitFile() { ma_encoder_uninit(&_encoder); }

  void warmup(CBContext *context) {
    _filename.warmup(context);

    if (!_filename.isVariable() && _filename->valueType == CBType::String) {
      const auto fname = CBSTRVIEW(_filename.get());
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

  CBVar activate(CBContext *context, const CBVar &input) {
    if (input.payload.audioValue.channels != _channels) {
      throw ActivationError("Input has an invalid number of audio channels");
    }
    ma_encoder_write_pcm_frames(&_encoder, input.payload.audioValue.samples,
                                input.payload.audioValue.nsamples);
    return input;
  }
};

void registerBlocks() {
  REGISTER_CBLOCK("Audio.Device", Device);
  REGISTER_CBLOCK("Audio.Channel", Channel);
  REGISTER_CBLOCK("Audio.Oscillator", Oscillator);
  REGISTER_CBLOCK("Audio.ReadFile", ReadFile);
  REGISTER_CBLOCK("Audio.WriteFile", WriteFile);
}
} // namespace Audio
} // namespace chainblocks