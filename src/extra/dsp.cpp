/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

#include "blocks/shared.hpp"
#include "runtime.hpp"

#include "../../deps/kissfft/kiss_fft.h"
#include "../../deps/kissfft/kiss_fftr.h"

// TODO optimize.
// Best would be to introduce our CBVar packing in kissfft fork
// Just like linalg, so to use our types directly

namespace chainblocks {
namespace DSP {
static TableVar experimental{{"experimental", Var(true)}};

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

  static const CBTable *properties() {
    return &experimental.payload.tableValue;
  }

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
  bool _asAudio{false};
  bool _complex{false};

  static CBTypesInfo inputTypes() {
    return CoreInfo::Float2SeqType;
  } // complex numbers

  static CBTypesInfo outputTypes() { return FloatTypes; }

  static const CBTable *properties() {
    return &experimental.payload.tableValue;
  }

  static inline Parameters Params{
      {"Audio",
       CBCCSTR("If the output should be an Audio chunk."),
       {CoreInfo::BoolType}},
      {"Complex",
       CBCCSTR("If the output should be complex numbers (only if not Audio)."),
       {CoreInfo::BoolType}}};

  CBParametersInfo parameters() { return Params; }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _asAudio = value.payload.boolValue;
      break;
    case 1:
      _complex = value.payload.boolValue;
      break;
    default:
      throw InvalidParameterIndex();
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_asAudio);
    case 1:
      return Var(_complex);
    default:
      throw InvalidParameterIndex();
    }
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    if (_asAudio) {
      OVERRIDE_ACTIVATE(data, activateAudio);
      return CoreInfo::AudioType;
    } else if (!_complex) {
      OVERRIDE_ACTIVATE(data, activateFloat);
      return CoreInfo::FloatSeqType;
    } else {
      OVERRIDE_ACTIVATE(data, activate);
      return CoreInfo::Float2SeqType;
    }
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
      if constexpr (OTYPE == CBType::Float) {
        _vscratch.resize(olen, CBVar{.valueType = OTYPE});
      }
      if constexpr (OTYPE == CBType::Float2) {
        _cscratch2.resize(len);
        _vscratch.resize(len, CBVar{.valueType = OTYPE});
      }
      if constexpr (OTYPE == CBType::Audio || OTYPE == CBType::Float) {
        _fscratch.resize(olen);
        _rstate = kiss_fftr_alloc(olen, 1, 0, 0);
        CBLOG_TRACE("IFFT alloc window {}", olen);
      } else {
        _state = kiss_fft_alloc(len, 1, 0, 0);
        CBLOG_TRACE("IFFT alloc window {}", len);
      }
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

#if 0
// TODO this works but we need to add more types, specifically orthogonal ones
// TODO also add coverage of all cases
struct WTBase {
  template <typename MAIN, typename FVARPAYLOAD> union BigFatFloat {
    MAIN v;
    FVARPAYLOAD f;
  };

  /**
   *  fwt97 - Forward biorthogonal 9/7 wavelet transform (lifting
   * implementation)
   *
   *  x is an input signal, which will be replaced by its output transform.
   *  n is the length of the signal, and must be a power of 2.
   *
   *  The first half part of the output signal contains the approximation
   * coefficients. The second half part contains the detail coefficients (aka.
   * the wavelets coefficients).
   *
   *  See also iwt97.
   */
  template <typename MAIN, typename FVARPAYLOAD>
  void fwt97(BigFatFloat<MAIN, FVARPAYLOAD> *x, int n) {
    static thread_local std::vector<BigFatFloat<MAIN, FVARPAYLOAD>> tempbank;
    tempbank.resize(n, {.f = FVARPAYLOAD(0.0)});

    BigFatFloat<MAIN, FVARPAYLOAD> a{.f = FVARPAYLOAD(0.0)};
    int i;

    // Predict 1
    a.f = -1.586134342;
    for (i = 1; i < n - 2; i += 2) {
      x[i].f += a.f * (x[i - 1].f + x[i + 1].f);
    }
    x[n - 1].f += FVARPAYLOAD(2.0) * a.f * x[n - 2].f;

    // Update 1
    a.f = -0.05298011854;
    for (i = 2; i < n; i += 2) {
      x[i].f += a.f * (x[i - 1].f + x[i + 1].f);
    }
    x[0].f += FVARPAYLOAD(2.0) * a.f * x[1].f;

    // Predict 2
    a.f = 0.8829110762;
    for (i = 1; i < n - 2; i += 2) {
      x[i].f += a.f * (x[i - 1].f + x[i + 1].f);
    }
    x[n - 1].f += FVARPAYLOAD(2.0) * a.f * x[n - 2].f;

    // Update 2
    a.f = 0.4435068522;
    for (i = 2; i < n; i += 2) {
      x[i].f += a.f * (x[i - 1].f + x[i + 1].f);
    }
    x[0].f += FVARPAYLOAD(2.0) * a.f * x[1].f;

    // Scale
    a.f = 1 / 1.149604398;
    for (i = 0; i < n; i++) {
      if (i % 2)
        x[i].f *= a.f;
      else
        x[i].f /= a.f;
    }

    for (i = 0; i < n; i++) {
      if (i % 2 == 0)
        tempbank[i / 2] = x[i];
      else
        tempbank[n / 2 + i / 2] = x[i];
    }
    for (i = 0; i < n; i++)
      x[i].f = tempbank[i].f;
  }

  /**
   *  iwt97 - Inverse biorthogonal 9/7 wavelet transform
   *
   *  This is the inverse of fwt97 so that iwt97(fwt97(x,n),n)=x for every
   * signal x of length n.
   *
   *  See also fwt97.
   */
  template <typename MAIN, typename FVARPAYLOAD>
  void iwt97(BigFatFloat<MAIN, FVARPAYLOAD> *x, int n) {
    static thread_local std::vector<BigFatFloat<MAIN, FVARPAYLOAD>> tempbank;
    tempbank.resize(n, {.f = FVARPAYLOAD(0.0)});

    BigFatFloat<MAIN, FVARPAYLOAD> a{.f = FVARPAYLOAD(0.0)};
    int i;

    // Unpack
    for (i = 0; i < n / 2; i++) {
      tempbank[i * 2] = x[i];
      tempbank[i * 2 + 1] = x[i + n / 2];
    }
    for (i = 0; i < n; i++)
      x[i].f = tempbank[i].f;

    // Undo scale
    a.f = 1.149604398;
    for (i = 0; i < n; i++) {
      if (i % 2)
        x[i].f *= a.f;
      else
        x[i].f /= a.f;
    }

    // Undo update 2
    a.f = -0.4435068522;
    for (i = 2; i < n; i += 2) {
      x[i].f += a.f * (x[i - 1].f + x[i + 1].f);
    }
    x[0].f += FVARPAYLOAD(2.0) * a.f * x[1].f;

    // Undo predict 2
    a.f = -0.8829110762;
    for (i = 1; i < n - 2; i += 2) {
      x[i].f += a.f * (x[i - 1].f + x[i + 1].f);
    }
    x[n - 1].f += FVARPAYLOAD(2.0) * a.f * x[n - 2].f;

    // Undo update 1
    a.f = 0.05298011854;
    for (i = 2; i < n; i += 2) {
      x[i].f += a.f * (x[i - 1].f + x[i + 1].f);
    }
    x[0].f += FVARPAYLOAD(2.0) * a.f * x[1].f;

    // Undo predict 1
    a.f = 1.586134342;
    for (i = 1; i < n - 2; i += 2) {
      x[i].f += a.f * (x[i - 1].f + x[i + 1].f);
    }
    x[n - 1].f += FVARPAYLOAD(2.0) * a.f * x[n - 2].f;
  }

  static CBTypesInfo inputTypes() { return CoreInfo::FloatSeqsOrAudio; }
  static CBTypesInfo outputTypes() { return CoreInfo::FloatSeqsOrAudio; }

  CBTypeInfo compose(const CBInstanceData &data) { return data.inputType; }
};

struct WT : public WTBase {
  static CBOptionalString help() {
    return CBCCSTR(
        "Computes in-place (so replacing the input values, keep a copy if "
        "needed) the biorthogonal 9/7 wavelet transform of the input sequence. "
        "Outputs the same sequence where the first half part of the output "
        "signal contains the approximation coefficients. The second half part "
        "contains the detail coefficients (aka. the wavelets coefficients).");
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (input.valueType == CBType::Audio) {
      fwt97(reinterpret_cast<BigFatFloat<float, float> *>(
                &input.payload.audioValue.samples[0]),
            input.payload.audioValue.nsamples);
      return input;
    } else {
      if (input.payload.seqValue.len == 0)
        return input;

      if (input.payload.seqValue.len % 2) {
        throw ActivationError("Input sequence must be dividible by two.");
      }

      switch (input.payload.seqValue.elements[0].valueType) {
      case CBType::Float:
        fwt97(reinterpret_cast<BigFatFloat<CBVar, FloatVarPayload> *>(
                  &input.payload.seqValue.elements[0]),
              input.payload.seqValue.len);
        return input;
      case CBType::Float2:
        fwt97(reinterpret_cast<BigFatFloat<CBVar, Float2VarPayload> *>(
                  &input.payload.seqValue.elements[0]),
              input.payload.seqValue.len);
        return input;
      case CBType::Float3:
        fwt97(reinterpret_cast<BigFatFloat<CBVar, Float3VarPayload> *>(
                  &input.payload.seqValue.elements[0]),
              input.payload.seqValue.len);
        return input;
      case CBType::Float4:
        fwt97(reinterpret_cast<BigFatFloat<CBVar, Float4VarPayload> *>(
                  &input.payload.seqValue.elements[0]),
              input.payload.seqValue.len);
        return input;
      default:
        throw ActivationError("Invalid input type");
      }
    }
  }
};

struct IWT : public WTBase {
  static CBOptionalString help() {
    return CBCCSTR(
        "Computes in-place (so replacing the input values, keep a copy if "
        "needed) the inverse biorthogonal 9/7 wavelet transform of the "
        "input sequence. Outputs the same sequence where the first half "
        "part of the output signal contains the approximation "
        "coefficients. The second half part contains the detail "
        "coefficients (aka. the wavelets coefficients).");
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (input.valueType == CBType::Audio) {
      iwt97(reinterpret_cast<BigFatFloat<float, float> *>(
                &input.payload.audioValue.samples[0]),
            input.payload.audioValue.nsamples);
      return input;
    } else {
      if (input.payload.seqValue.len == 0)
        return input;

      if (input.payload.seqValue.len % 2) {
        throw ActivationError("Input sequence must be dividible by two.");
      }

      switch (input.payload.seqValue.elements[0].valueType) {
      case CBType::Float:
        iwt97(reinterpret_cast<BigFatFloat<CBVar, FloatVarPayload> *>(
                  &input.payload.seqValue.elements[0]),
              input.payload.seqValue.len);
        return input;
      case CBType::Float2:
        iwt97(reinterpret_cast<BigFatFloat<CBVar, Float2VarPayload> *>(
                  &input.payload.seqValue.elements[0]),
              input.payload.seqValue.len);
        return input;
      case CBType::Float3:
        iwt97(reinterpret_cast<BigFatFloat<CBVar, Float3VarPayload> *>(
                  &input.payload.seqValue.elements[0]),
              input.payload.seqValue.len);
        return input;
      case CBType::Float4:
        iwt97(reinterpret_cast<BigFatFloat<CBVar, Float4VarPayload> *>(
                  &input.payload.seqValue.elements[0]),
              input.payload.seqValue.len);
        return input;
      default:
        throw ActivationError("Invalid input type");
      }
    }
  }
};
#endif

void registerBlocks() {
  REGISTER_CBLOCK("DSP.FFT", FFT);
  REGISTER_CBLOCK("DSP.IFFT", IFFT);
#if 0
  REGISTER_CBLOCK("DSP.Wavelet", WT);
  REGISTER_CBLOCK("DSP.InverseWavelet", IWT);
#endif
}
} // namespace DSP
} // namespace chainblocks