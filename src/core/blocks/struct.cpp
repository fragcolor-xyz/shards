/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

#include "shared.hpp"
#include <regex>

namespace chainblocks {
class Tokenizer {
public:
  Tokenizer(const std::string &input, const std::vector<std::regex> &regexes)
      : _tag(-1), _regs(regexes), _iter(input.begin()), _end(input.end()) {
    advance();
  }

  std::string token() const { return _token; }

  int tag() const { return _tag; };

  void next() { advance(); }

  bool eof() const { return _iter == _end; }

private:
  static inline std::regex whitespacex{"[\\s,]+|;.*"};

  void skipSpaces() {
    std::smatch match;
    auto flags = std::regex_constants::match_continuous;
    while (std::regex_search(_iter, _end, match, whitespacex, flags)) {
      _iter += match.str(0).size();
    }
  }

  void advance() {
    // skip current token, previosly consumed
    _iter += _token.size();

    skipSpaces();
    if (eof()) {
      return;
    }

    bool mismatched = true;
    auto tag = 0;
    for (auto &re : _regs) {
      std::smatch match;
      auto flags = std::regex_constants::match_continuous;
      if (std::regex_search(_iter, _end, match, re, flags)) {
        // matched
        _token = match.str(0);
        _tag = tag;
        mismatched = false;
        break;
      }
      tag++;
    }

    if (mismatched) {
      std::string mismatch(_iter, _end);
      throw CBException("Tokenizer mismatched, unexpected: " + mismatch);
    }
  }

  using Iterator = std::string::const_iterator;

  std::string _token;
  int _tag;
  const std::vector<std::regex> &_regs;
  Iterator _iter;
  Iterator _end;
};

// e.g i32 f32 b i8[256]

struct StructBase {
  enum Tags {
    i8Array,
    i16Array,
    i32Array,
    i64Array,
    f32Array,
    f64Array,
    i8,
    i16,
    i32,
    i64,
    f32,
    f64,
    Bool,
    Pointer,
    String
  };

  struct Desc {
    size_t arrlen;
    size_t offset;
    Tags tag;
  };

  static inline std::vector<std::regex> rexes{
      std::regex("i8\\[\\d+\\]"),  // i8 array
      std::regex("i16\\[\\d+\\]"), // i16 array
      std::regex("i32\\[\\d+\\]"), // i32 array
      std::regex("i64\\[\\d+\\]"), // i64 array
      std::regex("f32\\[\\d+\\]"), // f32 array
      std::regex("f64\\[\\d+\\]"), // f64 array
      std::regex("i8"),            // i8
      std::regex("i16"),           // i16
      std::regex("i32"),           // i32
      std::regex("i64"),           // i64
      std::regex("f32"),           // f32
      std::regex("f64"),           // f64
      std::regex("b"),             // bool
      std::regex("p"),             // pointer
      std::regex("s"),             // string
  };

  static inline std::regex arrlenx{"^.*\\[(\\d+)\\]$"};

  static inline ParamsInfo params = ParamsInfo(ParamsInfo::Param(
      "Definition", "A string defining the struct e.g. \"i32 f32 b i8[256]\".",
      CBTypesInfo(CoreInfo::strInfo)));

  static CBParametersInfo parameters() { return CBParametersInfo(params); }

  std::string _def;
  std::vector<Desc> _members;
  size_t _size;

  void setParam(int index, CBVar value) {
    _def = value.payload.stringValue;

    // compile members
    _members.clear();
    _size = 0;
    Tokenizer t(_def, rexes);
    while (!t.eof()) {
      auto token = t.token();
      Desc d{};
      d.tag = static_cast<Tags>(t.tag());
      if (d.tag < i8) { // array
        // must populate d.arrlen
        // should not fail, if does throw which is ok
        std::smatch match;
        if (!std::regex_search(token, match, arrlenx)) {
          throw CBException("Unexpected struct compiler failure.");
        }
        d.arrlen = std::stoll(match.str(1));
      }

      // store offset (using _size)
      d.offset = _size;

      _members.push_back(d);

      // compute size
      switch (d.tag) {
      case Tags::i8Array:
        _size += d.arrlen;
        break;
      case Tags::i16Array:
        _size += 2 * d.arrlen;
        break;
      case Tags::f32Array:
      case Tags::i32Array:
        _size += 4 * d.arrlen;
        break;
      case Tags::f64Array:
      case Tags::i64Array:
        _size += 8 * d.arrlen;
        break;
      case Tags::i8:
        _size += 1;
        break;
      case Tags::i16:
        _size += 2;
        break;
      case Tags::f32:
      case Tags::i32:
        _size += 4;
        break;
      case Tags::f64:
      case Tags::i64:
        _size += 8;
        break;
      case Tags::Bool:
        _size += 1;
        break;
      case Tags::Pointer:
        _size += sizeof(uintptr_t);
        break;
      case Tags::String:
        _size += sizeof(uintptr_t);
        break;
      }

      t.next();
    }
  }

  CBVar getParam(int index) { return Var(_def); }
};

struct Pack : public StructBase {
  std::vector<uint8_t> _storage;

  static CBTypesInfo inputTypes() { return CBTypesInfo(CoreInfo::anySeqInfo); }
  static CBTypesInfo outputTypes() {
    return CBTypesInfo(SharedTypes::bytesInfo);
  }

  void setParam(int index, CBVar value) {
    StructBase::setParam(index, value);

    // prepare our backing memory
    _storage.resize(_size);
  }

  void ensureType(const CBVar &input, CBType wantedType) {
    if (input.valueType != wantedType) {
      throw CBException("Expected " + type2Name(wantedType) +
                        " instead was: " + type2Name(input.valueType));
    }
  }

  template <typename T, typename CT> void write(const CT input, size_t offset) {
    T x = static_cast<T>(input);
    memcpy(&_storage.front() + offset, &x, sizeof(T));
  }

  template <typename T, CBType CBT, typename CT>
  void writeMany(const CBSeq &input, CT CBVarPayload::*value, size_t offset,
                 size_t len) {
    if (len != (size_t)input.len) {
      throw CBException("Expected " + std::to_string(len) +
                        " size sequence as value");
    }

    for (size_t i = 0; i < len; i++) {
      auto &val = input.elements[i];
      ensureType(val, CBT);
      write<T, decltype(val.payload.*value)>(val.payload.*value,
                                             offset + (i * sizeof(T)));
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (_members.size() != (size_t)input.payload.seqValue.len) {
      throw CBException("Expected " + std::to_string(_members.size()) +
                        " members as input.");
    }

    auto idx = 0;
    auto &seq = input.payload.seqValue;
    for (auto &member : _members) {
      switch (member.tag) {
      case Tags::i8Array:
        ensureType(seq.elements[idx], Seq);
        writeMany<int8_t, Int>(seq.elements[idx].payload.seqValue,
                               &CBVarPayload::intValue, member.offset,
                               member.arrlen);
        break;
      case Tags::i8:
        ensureType(seq.elements[idx], Int);
        write<int8_t>(seq.elements[idx].payload.intValue, member.offset);
        break;
      case Tags::i16Array:
        ensureType(seq.elements[idx], Seq);
        writeMany<int16_t, Int>(seq.elements[idx].payload.seqValue,
                                &CBVarPayload::intValue, member.offset,
                                member.arrlen);
        break;
      case Tags::i16:
        ensureType(seq.elements[idx], Int);
        write<int16_t>(seq.elements[idx].payload.intValue, member.offset);
        break;
      case Tags::i32Array:
        ensureType(seq.elements[idx], Seq);
        writeMany<int32_t, Int>(seq.elements[idx].payload.seqValue,
                                &CBVarPayload::intValue, member.offset,
                                member.arrlen);
        break;
      case Tags::i32:
        ensureType(seq.elements[idx], Int);
        write<int32_t>(seq.elements[idx].payload.intValue, member.offset);
        break;
      case Tags::i64Array:
        ensureType(seq.elements[idx], Seq);
        writeMany<int64_t, Int>(seq.elements[idx].payload.seqValue,
                                &CBVarPayload::intValue, member.offset,
                                member.arrlen);
        break;
      case Tags::i64:
        ensureType(seq.elements[idx], Int);
        write<int64_t>(seq.elements[idx].payload.intValue, member.offset);
        break;
      case Tags::f32Array:
        ensureType(seq.elements[idx], Seq);
        writeMany<float, Float>(seq.elements[idx].payload.seqValue,
                                &CBVarPayload::floatValue, member.offset,
                                member.arrlen);
        break;
      case Tags::f32:
        ensureType(seq.elements[idx], Float);
        write<float>(seq.elements[idx].payload.floatValue, member.offset);
        break;
      case Tags::f64Array:
        ensureType(seq.elements[idx], Seq);
        writeMany<double, Float>(seq.elements[idx].payload.seqValue,
                                 &CBVarPayload::floatValue, member.offset,
                                 member.arrlen);
        break;
      case Tags::f64:
        ensureType(seq.elements[idx], Float);
        write<double>(seq.elements[idx].payload.floatValue, member.offset);
        break;
      case Tags::Bool:
        ensureType(seq.elements[idx], CBType::Bool);
        write<bool>(seq.elements[idx].payload.boolValue, member.offset);
        break;
      case Tags::Pointer:
        ensureType(seq.elements[idx], Int);
        write<uintptr_t>(seq.elements[idx].payload.intValue, member.offset);
        break;
      case Tags::String:
        ensureType(seq.elements[idx], CBType::String);
        write<const char *>(seq.elements[idx].payload.stringValue,
                            member.offset);
      }
      idx++;
    }

    return Var(&_storage.front(), _size);
  }
};

// Register
RUNTIME_CORE_BLOCK(Pack);
RUNTIME_BLOCK_inputTypes(Pack);
RUNTIME_BLOCK_outputTypes(Pack);
RUNTIME_BLOCK_parameters(Pack);
RUNTIME_BLOCK_setParam(Pack);
RUNTIME_BLOCK_getParam(Pack);
RUNTIME_BLOCK_activate(Pack);
RUNTIME_BLOCK_END(Pack);

struct Unpack : public StructBase {
  CBSeq _output;

  static CBTypesInfo inputTypes() {
    return CBTypesInfo(SharedTypes::bytesInfo);
  }
  static CBTypesInfo outputTypes() { return CBTypesInfo(CoreInfo::anySeqInfo); }

  void destroy() {
    if (_output.elements) {
      // cleanup sub seqs
      for (size_t i = 0; i < _members.size(); i++) {
        if (_output.elements[i].valueType == Seq) {
          chainblocks::arrayFree(_output.elements[i].payload.seqValue);
        }
      }
      chainblocks::arrayFree(_output);
    }
  }

  void setParam(int index, CBVar value) {
    StructBase::setParam(index, value);
    // now we know what size we need
    size_t curLen = _output.len;
    if (_members.size() < curLen) {
      // need to destroy leftovers if arrays
      for (size_t i = _members.size(); i < curLen; i++) {
        if (_output.elements[i].valueType == Seq) {
          chainblocks::arrayFree(_output.elements[i].payload.seqValue);
        }
      }
    }
    chainblocks::arrayResize(_output, _members.size());
    auto idx = 0;
    for (auto &member : _members) {
      auto &arr = _output.elements[idx].payload.seqValue;
      switch (member.tag) {
      case Tags::i8Array:
      case Tags::i16Array:
      case Tags::i32Array:
      case Tags::i64Array:
        _output.elements[idx].valueType = Seq;
        chainblocks::arrayResize(_output.elements[idx].payload.seqValue,
                                 member.arrlen);
        for (size_t i = 0; i < member.arrlen; i++) {
          arr.elements[i].valueType = Int;
        }
        break;
      case Tags::i8:
      case Tags::i16:
      case Tags::i32:
      case Tags::i64:
        _output.elements[idx].valueType = Int;
        break;
      case Tags::f32Array:
      case Tags::f64Array:
        _output.elements[idx].valueType = Seq;
        chainblocks::arrayResize(_output.elements[idx].payload.seqValue,
                                 member.arrlen);
        for (size_t i = 0; i < member.arrlen; i++) {
          arr.elements[i].valueType = Float;
        }
        break;
      case Tags::f32:
      case Tags::f64:
        _output.elements[idx].valueType = Float;
        break;
      case Tags::Bool:
        _output.elements[idx].valueType = CBType::Bool;
        break;
      case Tags::Pointer:
        _output.elements[idx].valueType = Int;
        break;
      case Tags::String:
        _output.elements[idx].valueType = CBType::String;
        break;
      }
      idx++;
    }
  }

  template <typename T, typename CT>
  void read(CT &output, const CBVar &input, size_t offset) {
    T x;
    memcpy(&x, input.payload.bytesValue + offset, sizeof(T));
    output = static_cast<CT>(x);
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    auto idx = 0;
    for (auto &member : _members) {
      auto &arr = _output.elements[idx].payload.seqValue;
      switch (member.tag) {
      case Tags::i8Array:
        for (size_t i = 0; i < member.arrlen; i++) {
          read<int8_t>(arr.elements[i].payload.intValue, input, member.offset);
        }
        break;
      case Tags::i8:
        read<int8_t>(_output.elements[idx].payload.intValue, input,
                     member.offset);
        break;
      case Tags::i16Array:
        for (size_t i = 0; i < member.arrlen; i++) {
          read<int16_t>(arr.elements[i].payload.intValue, input, member.offset);
        }
        break;
      case Tags::i16:
        read<int16_t>(_output.elements[idx].payload.intValue, input,
                      member.offset);
        break;
      case Tags::i32Array:
        for (size_t i = 0; i < member.arrlen; i++) {
          read<int32_t>(arr.elements[i].payload.intValue, input, member.offset);
        }
        break;
      case Tags::i32:
        read<int32_t>(_output.elements[idx].payload.intValue, input,
                      member.offset);
        break;
      case Tags::i64Array:
        for (size_t i = 0; i < member.arrlen; i++) {
          read<int64_t>(arr.elements[i].payload.intValue, input, member.offset);
        }
        break;
      case Tags::i64:
        read<int64_t>(_output.elements[idx].payload.intValue, input,
                      member.offset);
        break;
      case Tags::f32Array:
        for (size_t i = 0; i < member.arrlen; i++) {
          read<float>(arr.elements[i].payload.floatValue, input, member.offset);
        }
        break;
      case Tags::f32:
        read<float>(_output.elements[idx].payload.floatValue, input,
                    member.offset);
        break;
      case Tags::f64Array:
        for (size_t i = 0; i < member.arrlen; i++) {
          read<double>(arr.elements[i].payload.floatValue, input,
                       member.offset);
        }
        break;
      case Tags::f64:
        read<double>(_output.elements[idx].payload.floatValue, input,
                     member.offset);
        break;
      case Tags::Bool:
        read<bool>(_output.elements[idx].payload.boolValue, input,
                   member.offset);
        break;
      case Tags::Pointer:
        read<uintptr_t>(_output.elements[idx].payload.intValue, input,
                        member.offset);
        break;
      case Tags::String:
        read<const char *>(_output.elements[idx].payload.stringValue, input,
                           member.offset);
        break;
      }
      idx++;
    }

    return Var(_output);
  }
};

// Register
RUNTIME_CORE_BLOCK(Unpack);
RUNTIME_BLOCK_destroy(Unpack);
RUNTIME_BLOCK_inputTypes(Unpack);
RUNTIME_BLOCK_outputTypes(Unpack);
RUNTIME_BLOCK_parameters(Unpack);
RUNTIME_BLOCK_setParam(Unpack);
RUNTIME_BLOCK_getParam(Unpack);
RUNTIME_BLOCK_activate(Unpack);
RUNTIME_BLOCK_END(Unpack);

void registerStructBlocks() {
  REGISTER_CORE_BLOCK(Pack);
  REGISTER_CORE_BLOCK(Unpack);
}
}; // namespace chainblocks
