/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2021 Giovanni Petrantoni */

#include "blocks/shared.hpp"
#include "runtime.hpp"

#include "../../deps/kissfft/kiss_fft.h"
#include "../../deps/kissfft/kiss_fftr.h"

// TODO optimize.. too many branches repeating.
// Also too many copies/conversions.. optimize this if perf is required

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

  CBTypeInfo compose(const CBInstanceData &data) {
    if (data.inputType.basicType == CBType::Audio) {
      OVERRIDE_ACTIVATE(data, activateAudio);
    } else {
      // assume seq
      if (data.inputType.seqTypes.elements[0].basicType == CBType::Float) {
        OVERRIDE_ACTIVATE(data, activateFloat);
      } else {
        OVERRIDE_ACTIVATE(data, activate);
      }
    }
    return CoreInfo::Float2SeqType;
  }

  template <CBType ITYPE>
  CBVar tactivate(CBContext *context, const CBVar &input) {
    int len = 0;
    if constexpr (ITYPE == CBType::Float || ITYPE == CBType::Float2) {
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

    int flen = 0;
    if constexpr (ITYPE == CBType::Audio || ITYPE == CBType::Float) {
      flen = (len / 2) + 1;
    } else {
      flen = len;
    }

    if (unlikely(_currentWindow != len)) {
      cleanup();

      if constexpr (ITYPE == CBType::Float2) {
        _state = kiss_fft_alloc(len, 0, 0, 0);
        _cscratch2.resize(len);
      } else if constexpr (ITYPE == CBType::Float) {
        _rstate = kiss_fftr_alloc(len, 0, 0, 0);
        _fscratch.resize(len);
      } else {
        _rstate = kiss_fftr_alloc(len, 0, 0, 0);
      }
      _currentWindow = len;
      _cscratch.resize(flen);
      _vscratch.resize(flen, CBVar{.valueType = CBType::Float2});
    }

    if constexpr (ITYPE == CBType::Float2) {
      int idx = 0;
      for (const auto &fvar : input) {
        _cscratch2[idx++] = {float(fvar.payload.float2Value[0]),
                             float(fvar.payload.float2Value[1])};
      }
      kiss_fft(_state, _cscratch2.data(), _cscratch.data());
    } else if constexpr (ITYPE == CBType::Float) {
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

  CBVar activateAudio(CBContext *context, const CBVar &input) {
    return tactivate<CBType::Audio>(context, input);
  }

  CBVar activateFloat(CBContext *context, const CBVar &input) {
    return tactivate<CBType::Float>(context, input);
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    return tactivate<CBType::Float2>(context, input);
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
        }
      }
    }
    // use generic complex
    OVERRIDE_ACTIVATE(data, activate);
    return CoreInfo::Float2SeqType;
  }

  template <CBType OTYPE>
  CBVar tactivate(CBContext *context, const CBVar &input) {
    const int len = int(input.payload.seqValue.len);
    if (len <= 0) {
      throw ActivationError("Expected a positive input length");
    }
    // following optimized away in certain paths
    const int olen = len * 2 - 2;

    if (unlikely(_currentWindow != len)) {
      cleanup();

      _currentWindow = len;
      _cscratch.resize(len);
      if constexpr (OTYPE == CBType::Float2 || OTYPE == CBType::Float) {
        _vscratch.resize(len, CBVar{.valueType = OTYPE});
      }
      if constexpr (OTYPE == CBType::Float2) {
        _cscratch2.resize(len);
      }
      if constexpr (OTYPE == CBType::Audio || OTYPE == CBType::Float) {
        _fscratch.resize(olen);
        _rstate = kiss_fftr_alloc(olen, 1, 0, 0);
      } else {
        _state = kiss_fft_alloc(len, 1, 0, 0);
      }

      CBLOG_TRACE("IFFT alloc window {}", len);
    }

    int idx = 0;
    for (const auto &vf : input) {
      _cscratch[idx++] = {float(vf.payload.float2Value[0]),
                          float(vf.payload.float2Value[1])};
    }

    if constexpr (OTYPE == CBType::Audio) {
      kiss_fftri(_rstate, _cscratch.data(), _fscratch.data());

      return Var(CBAudio{0, uint16_t(olen), uint16_t(1), _fscratch.data()});
    } else if constexpr (OTYPE == CBType::Float) {
      kiss_fftri(_rstate, _cscratch.data(), _fscratch.data());

      for (int i = 0; i < olen; i++) {
        _vscratch[i].payload.floatValue = double(_fscratch[i]);
      }

      return Var(_vscratch);
    } else {
      kiss_fft(_state, _cscratch.data(), _cscratch2.data());

      for (int i = 0; i < len; i++) {
        _vscratch[i].payload.float2Value[0] = _cscratch2[i].r;
        _vscratch[i].payload.float2Value[1] = _cscratch2[i].i;
      }

      return Var(_vscratch);
    }
  }

  CBVar activateFloat(CBContext *context, const CBVar &input) {
    return tactivate<CBType::Float>(context, input);
  }

  CBVar activateAudio(CBContext *context, const CBVar &input) {
    return tactivate<CBType::Audio>(context, input);
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    return tactivate<CBType::Float2>(context, input);
  }
};

void registerBlocks() {
  REGISTER_CBLOCK("DSP.FFT", FFT);
  REGISTER_CBLOCK("DSP.IFFT", IFFT);
}
} // namespace DSP
} // namespace chainblocks