/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#include <utf8.h>
#include <shards/core/shared.hpp>
#include <shards/core/module.hpp>
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
      _re_str = SHSTRVIEW(value);
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

  std::array<SHExposedTypeInfo, 1> _requiring;
  SHExposedTypesInfo requiredVariables() {
    if (_replacement.isVariable()) {
      _requiring[0].name = _replacement.variableName();
      _requiring[0].help = SHCCSTR("The required variable containing the replacement.");
      _requiring[0].exposedType = CoreInfo::StringType;
      return {_requiring.data(), 1, 0};
    } else {
      return {};
    }
  }

  void warmup(SHContext *context) { _replacement.warmup(context); }

  void cleanup(SHContext *context) { _replacement.cleanup(); }

  SHVar activate(SHContext *context, const SHVar &input) {
    _subject.assign(input.payload.stringValue, SHSTRLEN(input));
    _replacementStr.assign(_replacement.get().payload.stringValue, SHSTRLEN(_replacement.get()));
    _output.assign(std::regex_replace(_subject, _re, _replacementStr));
    return Var(_output);
  }
};

struct Format {
  static inline Type InputType = Type::SeqOf(CoreInfo::AnyType);

  static SHOptionalString help() { return SHCCSTR("Concatenates all the elements of a sequence into a string"); }

  static SHTypesInfo inputTypes() { return InputType; }
  static SHOptionalString inputHelp() {
    return SHCCSTR("A sequence of values that will be converted to string and joined together.");
  }

  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }
  static SHOptionalString outputHelp() { return SHCCSTR("A string consisting of all the elements of the sequence."); }

  CachedStreamBuf _buffer;

  SHVar activate(SHContext *context, const SHVar &input) {
    if (input.payload.seqValue.len == 0)
      return Var("");

    std::ostream stream(&_buffer);
    _buffer.reset();
    for (auto &v : input.payload.seqValue) {
      stream << v;
    }
    _buffer.done();
    return Var(_buffer.str());
  }
};

struct Join {
  static inline Type InputType = Type::SeqOf(CoreInfo::StringOrBytes);

  static SHOptionalString help() {
    return SHCCSTR("Concatenates all the elements of a string sequence, using the specified separator between each element.");
  }

  static SHTypesInfo inputTypes() { return InputType; }
  static SHOptionalString inputHelp() { return SHCCSTR("A sequence of string values that will be joined together."); }

  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }
  static SHOptionalString outputHelp() {
    return SHCCSTR("A string consisting of all the elements of the sequence delimited by the separator.");
  }

  static SHParametersInfo parameters() { return SHParametersInfo(params); }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _separator = SHSTRVIEW(value);
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
      if (input.payload.seqValue.elements[i].valueType != SHType::String &&
          input.payload.seqValue.elements[i].valueType != SHType::Bytes)
        throw ActivationError("Join: All elements of the input sequence must be strings or bytes.");
      _buffer.append(_separator);
      _buffer.append(input.payload.seqValue.elements[i].payload.stringValue, SHSTRLEN(input.payload.seqValue.elements[i]));
    }

    return Var(_buffer);
  }

private:
  static inline Parameters params{{"Separator", SHCCSTR("The string to use as a separator."), {CoreInfo::StringType}}};

  std::string _buffer;
  std::string _separator;
};

struct ToUpper {
  static SHOptionalString help() { return SHCCSTR("Converts a string to uppercase"); }

  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHOptionalString inputHelp() { return SHCCSTR("A string."); }

  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }
  static SHOptionalString outputHelp() { return SHCCSTR("A string in uppercase."); }

  OwnedVar nullTermBuffer;

  SHVar activate(SHContext *context, const SHVar &input) {
    nullTermBuffer = input;
    utf8upr(const_cast<char *>(nullTermBuffer.payload.stringValue));
    return nullTermBuffer;
  }
};

struct ToLower {
  static SHOptionalString help() { return SHCCSTR("Converts a string to lowercase"); }

  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHOptionalString inputHelp() { return SHCCSTR("A string."); }

  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }
  static SHOptionalString outputHelp() { return SHCCSTR("A string in lowercase."); }

  OwnedVar nullTermBuffer;

  SHVar activate(SHContext *context, const SHVar &input) {
    nullTermBuffer = input;
    utf8lwr(const_cast<char *>(nullTermBuffer.payload.stringValue));
    return nullTermBuffer;
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

struct Contains {
  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::BoolType; }

  ParamVar _check{Var("")};

  static inline Parameters params{{{"String",
                                    SHCCSTR("The string that needs to be contained by the input string to output true."),
                                    {CoreInfo::StringType, CoreInfo::StringVarType}}}};

  static SHParametersInfo parameters() { return SHParametersInfo(params); }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _check = value;
      break;
    default:
      throw InvalidParameterIndex();
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return _check;
    default:
      throw InvalidParameterIndex();
    }
  }

  std::array<SHExposedTypeInfo, 1> _requiring;
  SHExposedTypesInfo requiredVariables() {
    if (_check.isVariable()) {
      _requiring[0].name = _check.variableName();
      _requiring[0].help = SHCCSTR("The required variable containing the replacement.");
      _requiring[0].exposedType = CoreInfo::StringType;
      return {_requiring.data(), 1, 0};
    } else {
      return {};
    }
  }

  void warmup(SHContext *context) { _check.warmup(context); }
  void cleanup(SHContext *context) { _check.cleanup(); }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto sv = SHSTRVIEW(input);
    auto check = _check.get();
    auto checkSv = SHSTRVIEW(check);
    if (sv.find(checkSv) != std::string_view::npos) {
      return Var(true);
    } else {
      return Var(false);
    }
  }
};

struct StartsWith : Contains {
  static inline Parameters params{
      {{"With",
        SHCCSTR("The string that needs to start at the beginning of the input string to output true."),
        {CoreInfo::StringType, CoreInfo::StringVarType}}}};

  static SHParametersInfo parameters() { return SHParametersInfo(params); }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto sv = SHSTRVIEW(input);
    auto check = _check.get();
    auto checkSv = SHSTRVIEW(check);

    if (sv.size() >= checkSv.size() && sv.substr(0, checkSv.size()).compare(checkSv) == 0) {
      return Var(true);
    } else {
      return Var(false);
    }
  }
};

struct EndsWith : Contains {
  static inline Parameters params{{{"With",
                                    SHCCSTR("The string that needs to match the ending of the input string to output true."),
                                    {CoreInfo::StringType, CoreInfo::StringVarType}}}};

  static SHParametersInfo parameters() { return SHParametersInfo(params); }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto sv = SHSTRVIEW(input);
    auto check = _check.get();
    auto checkSv = SHSTRVIEW(check);

    if (sv.size() >= checkSv.size() && sv.substr(sv.size() - checkSv.size(), checkSv.size()).compare(checkSv) == 0) {
      return Var(true);
    } else {
      return Var(false);
    }
  }
};

struct Parser {
  std::string _cache;
};

struct ParseInt : public Parser {
  static SHOptionalString help() {
    return SHCCSTR("Converts the string representation of a number to a signed integer equivalent.");
  }

  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHOptionalString inputHelp() { return SHCCSTR("A string representing a number."); }

  static SHTypesInfo outputTypes() { return CoreInfo::IntType; }
  static SHOptionalString outputHelp() {
    return SHCCSTR("A signed integer equivalent to the number contained in the string input.");
  }

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

struct Split {
  std::string _inputStr;
  SeqVar _lines;
  std::string _separator;

  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringSeqType; }

  static inline Parameters params{{{"Separator",
                                    SHCCSTR("The separator character to split the string on."),
                                    {CoreInfo::StringType, CoreInfo::StringVarType}}}};

  static SHParametersInfo parameters() { return SHParametersInfo(params); }

  void setParam(int index, const SHVar &value) {
    switch (index) {
    case 0:
      _separator.clear();
      _separator.append(SHSTRVIEW(value)); // Accept a string of separators
      break;
    default:
      throw InvalidParameterIndex();
    }
  }

  SHVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_separator);
    default:
      throw InvalidParameterIndex();
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    auto input_string = SHSTRVIEW(input);
    _inputStr.clear();
    _inputStr.append(input_string);
    std::vector<std::string> components;
    size_t start = 0;
    size_t end = 0;

    _lines.clear();
    while ((end = _inputStr.find(_separator, start)) != std::string::npos) {
      if (end != start) { // Ignore empty tokens
        components.push_back(_inputStr.substr(start, end - start));
      }
      start = end + _separator.length();
    }
    if (start < _inputStr.length()) {
      components.push_back(_inputStr.substr(start));
    }

    for (const auto &component : components) {
      _lines.push_back(Var(component));
    }

    return _lines;
  }
};
} // namespace Regex
} // namespace shards
SHARDS_REGISTER_FN(strings) {
  using namespace shards::Regex;

  REGISTER_SHARD("Regex.Replace", Replace);
  REGISTER_SHARD("Regex.Search", Search);
  REGISTER_SHARD("Regex.Match", Match);
  REGISTER_SHARD("String.Format", Format);
  REGISTER_SHARD("String.Join", Join);
  REGISTER_SHARD("String.ToUpper", ToUpper);
  REGISTER_SHARD("String.ToLower", ToLower);
  REGISTER_SHARD("ParseInt", ParseInt);
  REGISTER_SHARD("ParseFloat", ParseFloat);
  REGISTER_SHARD("String.Trim", Trim);
  REGISTER_SHARD("String.Contains", Contains);
  REGISTER_SHARD("String.Split", Split);
  REGISTER_SHARD("String.Starts", StartsWith);
  REGISTER_SHARD("String.Ends", EndsWith);
}
