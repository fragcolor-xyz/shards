/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2021 Giovanni Petrantoni */

#include "blocks/shared.hpp"
#include "runtime.hpp"

#include "../../deps/kissfft/kiss_fft.h"
#include "../../deps/kissfft/kiss_fftr.h"

namespace chainblocks {
namespace DSP {
struct FFTBase {
  kiss_fft_cfg _state{nullptr};
  kiss_fftr_cfg _rstate{nullptr};
  int _currentWindow{-1};
  std::vector<kiss_fft_cpx> _cscratch;
  std::vector<kiss_fft_cpx> _cscratch2;
  std::vector<float> _fscratch;
  std::vector<CBVar> _vscratch;

  static inline Types FloatTypes{
      {CoreInfo::FloatSeqType, CoreInfo::Float2SeqType, CoreInfo::AudioType}};

  void cleanup() {
    if (_state) {
      kiss_fft_free(_state);
      _state = nullptr;
    }

    if (_rstate) {
      kiss_fftr_free(_rstate);
      _rstate = nullptr;
    }
  }
};

struct FFT : public FFTBase {
  static CBTypesInfo inputTypes() { return FloatTypes; }

  static CBTypesInfo outputTypes() {
    return CoreInfo::Float2SeqType;
  } // complex numbers

  CBVar activate(CBContext *context, const CBVar &input) {
    // TODO optimize.. too many branches repeating.
    // Use activation override
    // Also too many copies/conversions.. optimize this if perf is required
    int len = 0;
    if (input.valueType == CBType::Seq) {
      len = int(input.payload.seqValue.len);
    } else {
      // Audio
      len = int(input.payload.audioValue.nsamples);
      if (input.payload.audioValue.channels != 1) {
        throw ActivationError("FFT expects a single channel audio buffer");
      }
    }

    if (len <= 0) {
      throw ActivationError("Expected a positive input length");
    }

    int flen = (len / 2) + 1;

    if (unlikely(_currentWindow != len)) {
      cleanup();

      if (input.valueType == CBType::Seq &&
          input.payload.seqValue.elements[0].valueType == CBType::Float2) {
        _state = kiss_fft_alloc(len, 0, 0, 0);
        _cscratch2.resize(len);
      } else if (input.valueType == CBType::Seq &&
                 input.payload.seqValue.elements[0].valueType ==
                     CBType::Float) {
        _rstate = kiss_fftr_alloc(len, 0, 0, 0);
        _fscratch.resize(len);
      } else {
        _rstate = kiss_fftr_alloc(len, 0, 0, 0);
      }
      _currentWindow = len;
      _cscratch.resize(flen);
      _vscratch.resize(flen);

      for (int i = 0; i < flen; i++) {
        _vscratch[i].valueType = CBType::Float2;
      }
    }

    if (input.valueType == CBType::Seq &&
        input.payload.seqValue.elements[0].valueType == CBType::Float2) {
      int idx = 0;
      for (const auto &fvar : input) {
        _cscratch2[idx++] = {float(fvar.payload.float2Value[0]),
                             float(fvar.payload.float2Value[1])};
      }
      kiss_fft(_state, _cscratch2.data(), _cscratch.data());
    } else if (input.valueType == CBType::Seq &&
               input.payload.seqValue.elements[0].valueType == CBType::Float) {
      int idx = 0;
      for (const auto &fvar : input) {
        _fscratch[idx++] = float(fvar.payload.floatValue);
      }
      kiss_fftr(_rstate, _fscratch.data(), _cscratch.data());
    } else {
      kiss_fftr(_rstate, input.payload.audioValue.samples, _cscratch.data());
    }

    for (int i = 0; i < flen; i++) {
      _vscratch[i].payload.float2Value[0] = _cscratch[i].r;
      _vscratch[i].payload.float2Value[1] = _cscratch[i].i;
    }

    return Var(_vscratch);
  }
};

struct IFFT : public FFTBase {
  static CBTypesInfo inputTypes() {
    return CoreInfo::Float2SeqType;
  } // complex numbers

  static CBTypesInfo outputTypes() { return FloatTypes; }

  CBTypeInfo compose(const CBInstanceData &data) {
    const auto &nextTypes = data.outputTypes;
    if (nextTypes.len == 1) {
      // alright we can pick the type properly
      if (nextTypes.elements[0].basicType == CBType::Audio) {
        OVERRIDE_ACTIVATE(data, activateAudio);
        return CoreInfo::AudioType;
      } else if (nextTypes.elements[0].basicType == CBType::Seq) {
        if (nextTypes.elements[0].seqTypes.elements[0].basicType ==
            CBType::Float) {
          OVERRIDE_ACTIVATE(data, activateFloat);
          return CoreInfo::FloatSeqType;
        } else {
          OVERRIDE_ACTIVATE(data, activate);
          return CoreInfo::Float2SeqType;
        }
      }
    }
    // use generic complex
    OVERRIDE_ACTIVATE(data, activateFloat);
    return CoreInfo::FloatSeqType;
  }

  CBVar activateFloat(CBContext *context, const CBVar &input) {
    const int len = int(input.payload.seqValue.len);
    if (len <= 0) {
      throw ActivationError("Expected a positive input length");
    }
    const int olen = len * 2 - 2;

    if (unlikely(_currentWindow != olen)) {
      cleanup();

      _currentWindow = olen;
      _cscratch.resize(len);
      _fscratch.resize(olen);
      _vscratch.resize(olen);
      _rstate = kiss_fftr_alloc(olen, 1, 0, 0);
      CBLOG_TRACE("IFFT Float alloc window {}", olen);

      for (int i = 0; i < olen; i++) {
        _vscratch[i].valueType = CBType::Float;
      }
    }

    int idx = 0;
    for (const auto &vf : input) {
      _cscratch[idx++] = {float(vf.payload.float2Value[0]),
                          float(vf.payload.float2Value[1])};
    }

    kiss_fftri(_rstate, _cscratch.data(), _fscratch.data());

    for (int i = 0; i < olen; i++) {
      _vscratch[i].payload.floatValue = double(_fscratch[i]);
    }

    return Var(_vscratch);
  }

  CBVar activateAudio(CBContext *context, const CBVar &input) {
    const int len = int(input.payload.seqValue.len);
    if (len <= 0) {
      throw ActivationError("Expected a positive input length");
    }
    const int olen = len * 2 - 2;

    if (unlikely(_currentWindow != olen)) {
      cleanup();

      _currentWindow = olen;
      _cscratch.resize(len);
      _fscratch.resize(olen);
      _rstate = kiss_fftr_alloc(olen, 1, 0, 0);
      CBLOG_TRACE("IFFT Audio alloc window {}", olen);
    }

    int idx = 0;
    for (const auto &vf : input) {
      _cscratch[idx++] = {float(vf.payload.float2Value[0]),
                          float(vf.payload.float2Value[1])};
    }

    kiss_fftri(_rstate, _cscratch.data(), _fscratch.data());

    return Var(
        CBAudio{0, uint16_t(_currentWindow), uint16_t(1), _fscratch.data()});
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    const int len = int(input.payload.seqValue.len);
    if (len <= 0) {
      throw ActivationError("Expected a positive input length");
    }
    const int olen = len * 2 - 2;

    if (unlikely(_currentWindow != olen)) {
      cleanup();

      _currentWindow = olen;
      _cscratch.resize(len);
      _cscratch2.resize(olen);
      _vscratch.resize(olen);
      _state = kiss_fft_alloc(olen, 1, 0, 0);
      CBLOG_TRACE("IFFT Float2 alloc window {}", olen);

      for (int i = 0; i < olen; i++) {
        _vscratch[i].valueType = CBType::Float2;
      }
    }

    int idx = 0;
    for (const auto &vf : input) {
      _cscratch[idx++] = {float(vf.payload.float2Value[0]),
                          float(vf.payload.float2Value[1])};
    }

    kiss_fft(_state, _cscratch.data(), _cscratch2.data());

    for (int i = 0; i < olen; i++) {
      _vscratch[i].payload.float2Value[0] = _cscratch2[i].r;
      _vscratch[i].payload.float2Value[1] = _cscratch2[i].i;
    }

    return Var(_vscratch);
  }
};

void registerBlocks() {
  REGISTER_CBLOCK("DSP.FFT", FFT);
  REGISTER_CBLOCK("DSP.IFFT", IFFT);
}
} // namespace DSP
} // namespace chainblocks