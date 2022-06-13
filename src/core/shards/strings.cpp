/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include "../../../deps/utf8.h/utf8.h"
#include "shared.hpp"
#include <regex>

namespace shards {
namespace Regex {
struct Common {
  static inline Parameters params{{"Regex", SHCCSTR("The regular expression."), {CoreInfo::StringType}}};

  std::regex _re;
  std::string _re_str;
  std::string _subject;

  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }

  static SHParametersInfo parameters() { return SHParametersInfo(params); }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _re_str = value.payload.stringValue;
      _re.assign(_re_str);
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
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

  static SHTypesInfo outputTypes() { return CoreInfo::StringSeqType; }

  SHVar activate(SHContext *context, const SHVar &input) {
    std::smatch match;
    _subject.assign(input.payload.stringValue, SHSTRLEN(input));
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
    return Var(SHSeq(_output));
  }
};

struct Search : public Common {
  IterableSeq _output;
  std::vector<std::string> _pool;

  static SHTypesInfo outputTypes() { return CoreInfo::StringSeqType; }

  SHVar activate(SHContext *context, const SHVar &input) {
    std::smatch match;
    _subject.assign(input.payload.stringValue, SHSTRLEN(input));
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
    return Var(SHSeq(_output));
  }
};

struct Replace : public Common {
  ParamVar _replacement;
  std::string _replacementStr;
  std::string _output;

  static inline Parameters params{
      Common::params, {{"Replacement", SHCCSTR("The replacement expression."), {CoreInfo::StringType, CoreInfo::StringVarType}}}};

  static SHParametersInfo parameters() { return params; }

  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 1:
      _replacement = value;
      break;
    default:
      Common::setParam(index, value);
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 1:
      return _replacement;
    default:
      return Common::getParam(index);
    }
  }

  void warmup(SHContext *context) { _replacement.warmup(context); }

  void cleanup() { _replacement.cleanup(); }

  SHVar activate(SHContext *context, const SHVar &input) {
    _subject.assign(input.payload.stringValue, SHSTRLEN(input));
    _replacementStr.assign(_replacement.get().payload.stringValue, SHSTRLEN(_replacement.get()));
    _output.assign(std::regex_replace(_subject, _re, _replacementStr));
    return Var(_output);
  }
};

struct Join {
  static SHTypesInfo inputTypes() { return CoreInfo::StringSeqType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }

  static SHParametersInfo parameters() { return SHParametersInfo(params); }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _separator = value.payload.stringValue;
      break;
    default:
      break;
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_separator);
    default:
      return Var::Empty;
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    if (input.payload.seqValue.len == 0)
      return Var("");

    _buffer.clear();
    _buffer.append(input.payload.seqValue.elements[0].payload.stringValue, SHSTRLEN(input.payload.seqValue.elements[0]));

    for (uint32_t i = 1; i < input.payload.seqValue.len; i++) {
      assert(input.payload.seqValue.elements[i].valueType == String);
      _buffer.append(_separator);
      _buffer.append(input.payload.seqValue.elements[i].payload.stringValue, SHSTRLEN(input.payload.seqValue.elements[i]));
    }

    return Var(_buffer);
  }

private:
  static inline Parameters params{{"Separator", SHCCSTR("The separator."), {CoreInfo::StringType}}};

  std::string _buffer;
  std::string _separator;
};

struct ToUpper {
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }
  SHVar activate(SHContext *context, const SHVar &input) {
    utf8upr(const_cast<char *>(input.payload.stringValue));
    return input;
  }
};

struct ToLower {
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }
  SHVar activate(SHContext *context, const SHVar &input) {
    utf8lwr(const_cast<char *>(input.payload.stringValue));
    return input;
  }
};

struct Trim {
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }

  static std::string_view trim(std::string_view s) {
    s.remove_prefix(std::min(s.find_first_not_of(" \t\r\v\n"), s.size()));
    s.remove_suffix(std::min(s.size() - s.find_last_not_of(" \t\r\v\n") - 1, s.size()));

    return s;
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto sv = SHSTRVIEW(input);
    auto tsv = trim(sv);
    return Var(tsv);
  }
};

struct Parser {
  std::string _cache;
};

struct ParseInt : public Parser {
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::IntType; }

  int _base{10};

  static inline Parameters params{{"Base",
                                   SHCCSTR("Numerical base (radix) that determines the valid characters "
                                           "and their interpretation."),
                                   {CoreInfo::IntType}}};
  SHParametersInfo parameters() { return params; }

  void setParam(int index, const SHVar &value) { _base = value.payload.intValue; }

  SHVar getParam(int index) { return Var(_base); }

  SHVar activate(SHContext *context, const SHVar &input) {
    char *str = const_cast<char *>(input.payload.stringValue);
    const auto len = SHSTRLEN(input);
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
  static SHOptionalString help() {
    return SHCCSTR("Converts the string representation of a number to a floating-point number equivalent.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHOptionalString inputHelp() { return SHCCSTR("A string representing a number."); }

  static SHTypesInfo outputTypes() { return CoreInfo::FloatType; }
  static SHOptionalString outputHelp() {
    return SHCCSTR("A floating-point number equivalent to the number contained in the string input.");
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    char *str = const_cast<char *>(input.payload.stringValue);
    const auto len = SHSTRLEN(input);
    _cache.assign(str, len);
    size_t parsed;
    auto v = std::stod(_cache, &parsed);
    if (parsed != len) {
      throw ActivationError("ParseFloat: Invalid float string");
    }
    return Var(v);
  }
};

void registerShards() {
  REGISTER_SHARD("Regex.Replace", Replace);
  REGISTER_SHARD("Regex.Search", Search);
  REGISTER_SHARD("Regex.Match", Match);
  REGISTER_SHARD("String.Join", Join);
  REGISTER_SHARD("String.ToUpper", ToUpper);
  REGISTER_SHARD("String.ToLower", ToLower);
  REGISTER_SHARD("ParseInt", ParseInt);
  REGISTER_SHARD("ParseFloat", ParseFloat);
  REGISTER_SHARD("String.Trim", Trim);
}
} // namespace Regex
} // namespace shards
