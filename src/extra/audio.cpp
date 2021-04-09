/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2021 Giovanni Petrantoni */

#include "blocks/shared.hpp"
#include "runtime.hpp"

#define STB_VORBIS_HEADER_ONLY
#include "extras/stb_vorbis.c" // Enables Vorbis decoding.

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

struct File {
  ma_decoder _decoder;
  bool _initialized{false};

  ma_uint32 _channels{2};
  ma_uint64 _nsamples{1024};
  ma_uint32 _sampleRate{44100};
  // what to do when not looped ends? throw?
  bool _looped{false};
  ParamVar _fromSample;
  ParamVar _toSample;
  ParamVar _filename;

  std::vector<float> _buffer;
  bool _done{false};

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
       CBCCSTR("The desired output sampling rate."),
       {CoreInfo::IntType}},
      {"Samples",
       CBCCSTR("The desired number of samples in the output."),
       {CoreInfo::IntType}},
      {"Looped",
       CBCCSTR("If the file should be played in loop or should stop the chain "
               "when it ends."),
       {CoreInfo::BoolType}},
      {"From",
       CBCCSTR("The starting sample index."),
       {CoreInfo::IntType, CoreInfo::IntVarType, CoreInfo::NoneType}},
      {"To",
       CBCCSTR("The final sample index (excluding)."),
       {CoreInfo::IntType, CoreInfo::IntVarType, CoreInfo::NoneType}}};

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
  }

  void cleanup() {
    _fromSample.cleanup();
    _toSample.cleanup();
    _filename.cleanup();

    if (_initialized) {
      deinitFile();
      _initialized = false;
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (unlikely(_done)) {
      if (_looped) {
        ma_result res = ma_decoder_seek_to_pcm_frame(&_decoder, 0);
        if (res != MA_SUCCESS) {
          throw ActivationError("Failed to seek");
        }
        _done = false;
      } else {
        context->stopFlow(Var::Empty);
      }
    }

    // read pcm data every iteration
    ma_uint64 framesRead =
        ma_decoder_read_pcm_frames(&_decoder, _buffer.data(), _nsamples);
    if (framesRead < _nsamples) {
      // Reached the end.
      _done = true;
      return Var(CBAudio{_sampleRate, uint16_t(framesRead), uint16_t(_channels),
                         _buffer.data()});
    } else {
      return Var(CBAudio{_sampleRate, uint16_t(_nsamples), uint16_t(_channels),
                         _buffer.data()});
    }
  }
};

void registerBlocks() { REGISTER_CBLOCK("Audio.File", File); }
} // namespace Audio
} // namespace chainblocks