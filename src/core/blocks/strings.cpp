/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "../../../deps/utf8.h/utf8.h"
#include "shared.hpp"
#include <regex>

namespace chainblocks {
namespace Regex {
struct Common {
  static inline Parameters params{{"Regex", CBCCSTR("The regular expression."), {CoreInfo::StringType}}};

  std::regex _re;
  std::string _re_str;
  std::string _subject;

  static CBTypesInfo inputTypes() { return CoreInfo::StringType; }

  static CBParametersInfo parameters() { return CBParametersInfo(params); }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _re_str = value.payload.stringValue;
      _re.assign(_re_str);
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_re_str);
    default:
      return Var::Empty;
    }
  }
};

struct Match : public Common {
  IterableSeq _output;
  std::vector<std::string> _pool;

  static CBTypesInfo outputTypes() { return CoreInfo::StringSeqType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    std::smatch match;
    _subject.assign(input.payload.stringValue, CBSTRLEN(input));
    if (std::regex_match(_subject, match, _re)) {
      auto size = match.size();
      _pool.resize(size);
      _output.resize(size);
      for (size_t i = 0; i < size; i++) {
        _pool[i].assign(match[i].str());
        _output[i] = Var(_pool[i]);
      }
    } else {
      _pool.clear();
      _output.clear();
    }
    return Var(CBSeq(_output));
  }
};

struct Search : public Common {
  IterableSeq _output;
  std::vector<std::string> _pool;

  static CBTypesInfo outputTypes() { return CoreInfo::StringSeqType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    std::smatch match;
    _subject.assign(input.payload.stringValue, CBSTRLEN(input));
    _pool.clear();
    _output.clear();
    while (std::regex_search(_subject, match, _re)) {
      auto size = match.size();
      for (size_t i = 0; i < size; i++) {
        _pool.emplace_back(match[i].str());
      }
      _subject.assign(match.suffix());
    }
    for (auto &s : _pool) {
      _output.push_back(Var(s));
    }
    return Var(CBSeq(_output));
  }
};

struct Replace : public Common {
  ParamVar _replacement;
  std::string _replacementStr;
  std::string _output;

  static inline Parameters params{
      Common::params, {{"Replacement", CBCCSTR("The replacement expression."), {CoreInfo::StringType, CoreInfo::StringVarType}}}};

  static CBParametersInfo parameters() { return params; }

  static CBTypesInfo outputTypes() { return CoreInfo::StringType; }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 1:
      _replacement = value;
      break;
    default:
      Common::setParam(index, value);
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 1:
      return _replacement;
    default:
      return Common::getParam(index);
    }
  }

  void warmup(CBContext *context) { _replacement.warmup(context); }

  void cleanup() { _replacement.cleanup(); }

  CBVar activate(CBContext *context, const CBVar &input) {
    _subject.assign(input.payload.stringValue, CBSTRLEN(input));
    _replacementStr.assign(_replacement.get().payload.stringValue, CBSTRLEN(_replacement.get()));
    _output.assign(std::regex_replace(_subject, _re, _replacementStr));
    return Var(_output);
  }
};

struct Join {
  static CBTypesInfo inputTypes() { return CoreInfo::StringSeqType; }
  static CBTypesInfo outputTypes() { return CoreInfo::StringType; }

  static CBParametersInfo parameters() { return CBParametersInfo(params); }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _separator = value.payload.stringValue;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_separator);
    default:
      return Var::Empty;
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (input.payload.seqValue.len == 0)
      return Var("");

    _buffer.clear();
    _buffer.append(input.payload.seqValue.elements[0].payload.stringValue, CBSTRLEN(input.payload.seqValue.elements[0]));

    for (uint32_t i = 1; i < input.payload.seqValue.len; i++) {
      assert(input.payload.seqValue.elements[i].valueType == String);
      _buffer.append(_separator);
      _buffer.append(input.payload.seqValue.elements[i].payload.stringValue, CBSTRLEN(input.payload.seqValue.elements[i]));
    }

    return Var(_buffer);
  }

private:
  static inline Parameters params{{"Separator", CBCCSTR("The separator."), {CoreInfo::StringType}}};

  std::string _buffer;
  std::string _separator;
};

struct ToUpper {
  static CBTypesInfo inputTypes() { return CoreInfo::StringType; }
  static CBTypesInfo outputTypes() { return CoreInfo::StringType; }
  CBVar activate(CBContext *context, const CBVar &input) {
    utf8upr(const_cast<char *>(input.payload.stringValue));
    return input;
  }
};

struct ToLower {
  static CBTypesInfo inputTypes() { return CoreInfo::StringType; }
  static CBTypesInfo outputTypes() { return CoreInfo::StringType; }
  CBVar activate(CBContext *context, const CBVar &input) {
    utf8lwr(const_cast<char *>(input.payload.stringValue));
    return input;
  }
};

struct Trim {
  static CBTypesInfo inputTypes() { return CoreInfo::StringType; }
  static CBTypesInfo outputTypes() { return CoreInfo::StringType; }

  static std::string_view trim(std::string_view s) {
    s.remove_prefix(std::min(s.find_first_not_of(" \t\r\v\n"), s.size()));
    s.remove_suffix(std::min(s.size() - s.find_last_not_of(" \t\r\v\n") - 1, s.size()));

    return s;
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    auto sv = CBSTRVIEW(input);
    auto tsv = trim(sv);
    return Var(tsv);
  }
};

struct Parser {
  std::string _cache;
};

struct ParseInt : public Parser {
  static CBTypesInfo inputTypes() { return CoreInfo::StringType; }
  static CBTypesInfo outputTypes() { return CoreInfo::IntType; }

  int _base{10};

  static inline Parameters params{{"Base",
                                   CBCCSTR("Numerical base (radix) that determines the valid characters "
                                           "and their interpretation."),
                                   {CoreInfo::IntType}}};
  CBParametersInfo parameters() { return params; }

  void setParam(int index, const CBVar &value) { _base = value.payload.intValue; }

  CBVar getParam(int index) { return Var(_base); }

  CBVar activate(CBContext *context, const CBVar &input) {
    char *str = const_cast<char *>(input.payload.stringValue);
    const auto len = CBSTRLEN(input);
    _cache.assign(str, len);
    size_t parsed;
    auto v = int64_t(std::stoull(_cache, &parsed, _base));
    if (parsed != len) {
      throw ActivationError("ParseInt: Invalid integer string");
    }
    return Var(v);
  }
};

struct ParseFloat : public Parser {
  static CBTypesInfo inputTypes() { return CoreInfo::StringType; }
  static CBTypesInfo outputTypes() { return CoreInfo::FloatType; }
  CBVar activate(CBContext *context, const CBVar &input) {
    char *str = const_cast<char *>(input.payload.stringValue);
    const auto len = CBSTRLEN(input);
    _cache.assign(str, len);
    size_t parsed;
    auto v = std::stod(_cache, &parsed);
    if (parsed != len) {
      throw ActivationError("ParseFloat: Invalid float string");
    }
    return Var(v);
  }
};

void registerBlocks() {
  REGISTER_CBLOCK("Regex.Replace", Replace);
  REGISTER_CBLOCK("Regex.Search", Search);
  REGISTER_CBLOCK("Regex.Match", Match);
  REGISTER_CBLOCK("String.Join", Join);
  REGISTER_CBLOCK("String.ToUpper", ToUpper);
  REGISTER_CBLOCK("String.ToLower", ToLower);
  REGISTER_CBLOCK("ParseInt", ParseInt);
  REGISTER_CBLOCK("ParseFloat", ParseFloat);
  REGISTER_CBLOCK("String.Trim", Trim);
}
} // namespace Regex
} // namespace chainblocks
