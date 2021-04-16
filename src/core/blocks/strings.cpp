/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2021 Giovanni Petrantoni */

#include "../../../deps/utf8.h/utf8.h"
#include "shared.hpp"
#include <regex>

namespace chainblocks {
namespace Regex {
struct Common {
  static inline ParamsInfo params = ParamsInfo(ParamsInfo::Param(
      "Regex", CBCCSTR("The regular expression."), CoreInfo::StringType));

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
  std::string _replacement;
  std::string _output;

  static inline ParamsInfo params = ParamsInfo(
      Common::params,
      ParamsInfo::Param("Replacement", CBCCSTR("The replacement expression."),
                        CoreInfo::StringType));

  static CBParametersInfo parameters() { return CBParametersInfo(params); }

  static CBTypesInfo outputTypes() { return CoreInfo::StringType; }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 1:
      _replacement = value.payload.stringValue;
      break;
    default:
      Common::setParam(index, value);
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 1:
      return Var(_replacement);
    default:
      return Common::getParam(index);
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    _subject.assign(input.payload.stringValue, CBSTRLEN(input));
    _output.assign(std::regex_replace(_subject, _re, _replacement));
    return Var(_output);
  }
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

struct Parser {
  std::string _cache;
};

struct ParseInt : public Parser {
  static CBTypesInfo inputTypes() { return CoreInfo::StringType; }
  static CBTypesInfo outputTypes() { return CoreInfo::IntType; }

  int _base{10};

  static inline Parameters params{
      {"Base",
       CBCCSTR("Numerical base (radix) that determines the valid characters "
               "and their interpretation."),
       {CoreInfo::IntType}}};
  CBParametersInfo parameters() { return params; }

  void setParam(int index, const CBVar &value) {
    _base = value.payload.intValue;
  }

  CBVar getParam(int index) { return Var(_base); }

  CBVar activate(CBContext *context, const CBVar &input) {
    char *str = const_cast<char *>(input.payload.stringValue);
    const auto len = CBSTRLEN(input);
    _cache.assign(str, len);
    auto v = int64_t(std::stoul(_cache, nullptr, _base));
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
    return Var(std::stod(_cache, nullptr));
  }
};

void registerBlocks() {
  REGISTER_CBLOCK("Regex.Replace", Replace);
  REGISTER_CBLOCK("Regex.Search", Search);
  REGISTER_CBLOCK("Regex.Match", Match);
  REGISTER_CBLOCK("String.ToUpper", ToUpper);
  REGISTER_CBLOCK("String.ToLower", ToLower);
  REGISTER_CBLOCK("ParseInt", ParseInt);
  REGISTER_CBLOCK("ParseFloat", ParseFloat);
}
} // namespace Regex
} // namespace chainblocks
