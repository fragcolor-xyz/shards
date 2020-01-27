/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

#include "shared.hpp"
#include <regex>

namespace chainblocks {
namespace Regex {
struct Common {
  static inline ParamsInfo params = ParamsInfo(ParamsInfo::Param(
      "Regex", "The regular expression.", CoreInfo::StringType));

  std::regex _re;
  std::string _re_str;

  static CBTypesInfo inputTypes() { return CoreInfo::StringType; }

  static CBParametersInfo parameters() { return CBParametersInfo(params); }

  void setParam(int index, CBVar value) {
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
      break;
    default:
      return CBVar();
    }
  }
};

struct Match : public Common {
  IterableSeq _output;
  std::vector<std::string> _pool;

  static CBTypesInfo outputTypes() { return CoreInfo::StringSeqType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    std::smatch match;
    std::string subject(input.payload.stringValue);
    if (std::regex_match(subject, match, _re)) {
      auto size = match.size();
      _pool.resize(size);
      _output.resize(size);
      for (size_t i = 0; i < size; i++) {
        _pool[i].assign(match[i].str());
        _output[i] = Var(_pool[i]);
      }
    }
    return Var(_output);
  }
};

typedef BlockWrapper<Match> MatchBlock;

struct Search : public Common {
  IterableSeq _output;
  std::vector<std::string> _pool;

  static CBTypesInfo outputTypes() { return CoreInfo::StringSeqType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    std::smatch match;
    std::string subject(input.payload.stringValue);
    _pool.clear();
    _output.clear();
    while (std::regex_search(subject, match, _re)) {
      auto size = match.size();
      for (size_t i = 0; i < size; i++) {
        _pool.emplace_back(match[i].str());
      }
      subject = match.suffix();
    }
    for (auto &s : _pool) {
      _output.push_back(Var(s));
    }
    return Var(_output);
  }
};

typedef BlockWrapper<Search> SearchBlock;

struct Replace : public Common {
  std::string _replacement;
  std::string _output;

  static inline ParamsInfo params =
      ParamsInfo(Common::params,
                 ParamsInfo::Param("Replacement", "The replacement expression.",
                                   CoreInfo::StringType));

  static CBParametersInfo parameters() { return CBParametersInfo(params); }

  static CBTypesInfo outputTypes() { return CoreInfo::StringType; }

  void setParam(int index, CBVar value) {
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
    _output.assign(
        std::regex_replace(input.payload.stringValue, _re, _replacement));
    return Var(_output);
  }
};

typedef BlockWrapper<Replace> ReplaceBlock;

void registerBlocks() {
  registerBlock("Regex.Match", &MatchBlock::create);
  registerBlock("Regex.Search", &SearchBlock::create);
  registerBlock("Regex.Replace", &ReplaceBlock::create);
}
} // namespace Regex
} // namespace chainblocks
