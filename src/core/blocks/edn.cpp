/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2021 Giovanni Petrantoni */

#include "../edn/print.hpp"
#include "../edn/read.hpp"
#include "shared.hpp"

namespace chainblocks {
namespace edn {
struct Uglify {
  std::string _output;
  static CBTypesInfo inputTypes() { return CoreInfo::StringType; }
  static CBTypesInfo outputTypes() { return CoreInfo::StringType; }
  CBVar activate(CBContext *context, const CBVar &input) {
    const auto s =
        input.payload.stringLen > 0
            ? std::string(input.payload.stringValue, input.payload.stringLen)
            : std::string(input.payload.stringValue);
    _output.assign(print(read(s)));
    return Var(_output);
  }
};

#if 0
struct Parse {
  std::vector<OwnedVar> _output;

  static CBTypesInfo inputTypes() { return CoreInfo::StringType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnySeqType; }

  OwnedVar to_var(token::Token &token) {
    switch (token.value.index()) {
    case token::value::BOOL:
      return Var(std::get<bool>(token.value));
    case token::value::CHAR:
      return Var(std::get<char>(token.value));
    case token::value::LONG:
      return Var(std::get<int64_t>(token.value));
    case token::value::DOUBLE:
      return Var(std::get<double>(token.value));
    case token::value::STRING: {
      return Var(std::get<std::string>(token.value));
    }
    }
    return Var::Empty;
  }

  template <class T> OwnedVar to_var(T &list) {
    std::vector<OwnedVar> vars;
    BOOST_FOREACH (auto &item, list) { vars.emplace_back(to_var(item)); }
    return Var(vars);
  }

  OwnedVar to_var(form::FormWrapper formWrapper) {
    return to_var(formWrapper.form);
  }

  CBMap to_var(form::FormWrapperMap &map) {
    CBMap res;
    BOOST_FOREACH (auto &item, map) {
      OwnedVar key = to_var(item.first.form);
      if (key.valueType != CBType::String)
        throw ActivationError("Map key must be a string");
      OwnedVar value = to_var(item.second.form);
      res[key.payload.stringValue] = value;
    }
    return res;
  }

  OwnedVar to_var(form::Form form) {
    switch (form.index()) {
    case form::SPECIAL: {
      auto error = std::get<form::Special>(form);
      LOG(ERROR) << "EDN parsing error: " << error.message;
      throw ActivationError("EDN parsing error");
    }
    case form::TOKEN:
      return to_var(std::get<token::Token>(form));
    case form::LIST:
      return to_var<std::list<form::FormWrapper>>(
          std::get<std::list<form::FormWrapper>>(form));
    case form::VECTOR:
      return to_var<std::vector<form::FormWrapper>>(
          std::get<std::vector<form::FormWrapper>>(form));
    case form::MAP: {
      CBMap m = to_var(std::get<form::FormWrapperMap>(form));
      CBVar tmp{};
      tmp.valueType = CBType::Table;
      tmp.payload.tableValue.api = &Globals::TableInterface;
      tmp.payload.tableValue.opaque = &m;
      return tmp;
    }
    case form::SET:
      return to_var<form::FormWrapperSet>(std::get<form::FormWrapperSet>(form));
    }
    return Var::Empty;
  }

  void to_var(const std::list<form::Form> &forms) {
    _output.clear();
    for (auto &form : forms) {
      _output.emplace_back(to_var(form));
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    const auto s =
        input.payload.stringLen > 0
            ? std::string(input.payload.stringValue, input.payload.stringLen)
            : std::string(input.payload.stringValue);

    _output.clear();

    const auto forms = read(s);
    to_var(forms);

    return Var(_output);
  }
};

struct Render {
  std::string _output;

  static CBTypesInfo inputTypes() { return CoreInfo::AnySeqType; }
  static CBTypesInfo outputTypes() { return CoreInfo::StringType; }

  CBVar activate(CBContext *context, const CBVar &input) {
    return Var(_output);
  }
};
#endif

void registerBlocks() {
  REGISTER_CBLOCK("EDN.Uglify", Uglify);
#if 0
  REGISTER_CBLOCK("EDN.Parse", Parse);
  REGISTER_CBLOCK("EDN.Render", Render);
#endif
}
} // namespace edn
} // namespace chainblocks