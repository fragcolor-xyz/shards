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
  ma_uint32 _sampleRate{44100};
  // what to do when not looped ends? throw?
  bool _looped{false};
  ParamVar _fromSample;
  ParamVar _toSample;
  ParamVar _nsamples{Var(1024)};
  ParamVar _filename;

  std::vector<float> _buffer;

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
       {CoreInfo::IntType, CoreInfo::IntVarType}},
      {"From",
       CBCCSTR("The starting sample index."),
       {CoreInfo::IntType, CoreInfo::IntVarType, CoreInfo::NoneType}},
      {"To",
       CBCCSTR("The final sample index (excluding)."),
       {CoreInfo::IntType, CoreInfo::IntVarType, CoreInfo::NoneType}}};

  void initFile(const std::string_view &filename) {
    ma_decoder_config config =
        ma_decoder_config_init(ma_format_f32, _channels, _sampleRate);
    ma_result res = ma_decoder_init_file(filename.data(), &config, &_decoder);
    if (res != MA_SUCCESS) {
      CBLOG_ERROR("Failed to open audio file {}", filename);
      throw ActivationError("Failed to open audio file");
    }
  }

  void warmup(CBContext *context) {
    _filename.warmup(context);

    if (!_filename.isVariable() && _filename->valueType == CBType::String) {
      const auto fname = CBSTRVIEW(_filename.get());
      initFile(fname);
      _initialized = true;
    }
  }
};

void registerBlocks() {}
} // namespace Audio
} // namespace chainblocks