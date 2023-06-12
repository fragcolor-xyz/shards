/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2021 Fragcolor Pte. Ltd. */

#include "print.hpp"
#include "read.hpp"
#include <shards/core/shared.hpp>

namespace shards {
namespace edn {
struct Uglify {
  std::string _output;
  std::vector<OwnedVar> _cases;
  std::vector<ShardsVar> _actions;
  std::vector<SHVar> _full;
  SHContext *_context{nullptr};

  static inline Parameters params{{"Hooks",
                                   SHCCSTR("A list of pairs to hook, [<symbol name> <shards to execute>], "
                                           "shards will have as input the contents of the symbols's list."),
                                   {CoreInfo::AnySeqType}}};
  static SHParametersInfo parameters() { return params; }

  void setParam(int index, const SHVar &value) {
    _cases.clear();
    _actions.clear();
    _full.clear();
    if (value.valueType == SHType::Seq) {
      auto counter = value.payload.seqValue.len;
      if (counter % 2)
        throw SHException("EDN.Uglify Hooks expected a sequence of pairs");
      _cases.resize(counter / 2);
      _actions.resize(counter / 2);
      _full.resize(counter);
      auto idx = 0;
      for (uint32_t i = 0; i < counter; i += 2) {
        _cases[idx] = value.payload.seqValue.elements[i];
        _actions[idx] = value.payload.seqValue.elements[i + 1];
        _full[i] = _cases[idx];
        _full[i + 1] = _actions[idx];
        idx++;
      }
    }
  }

  SHVar getParam(int index) {
    if (_full.size() == 0) {
      return Var::Empty;
    } else {
      return Var(_full);
    }
  }

  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }

  void rewrite(token::Token &token, SHVar &v) {
    switch (v.valueType) {
    case SHType::Bool:
      token.value = v.payload.boolValue;
      break;
    case SHType::Int:
      if (token::value::CHAR)
        token.value = char(v.payload.intValue);
      else
        token.value = v.payload.intValue;
      break;
    case SHType::Float:
      token.value = v.payload.floatValue;
      break;
    case SHType::String: {
      auto sv = SHSTRVIEW(v);
      token.value = std::string(sv);
    } break;
    default:
      SHLOG_ERROR("Type not handled: {}", type2Name(v.valueType));
      throw ActivationError("Type not handled");
    }
  }

  template <class T> void rewrite(T &seq, SHVar &v) {
    auto idx = 0;
    for (auto it = seq.begin(); it != seq.end();) {
      auto &val = *(begin(v) + idx);
      if (val.valueType == SHType::None) {
        // we want to remove this token
        it = seq.erase(it);
      } else {
        rewrite(*it, val);
        it++;
      }
      idx++;
    }
  }

  void rewrite(form::FormWrapper &formWrapper, SHVar &v) { rewrite(formWrapper.form, v); }

  void rewrite(form::FormWrapperMap &map, SHVar &v) { throw ActivationError("Map not handled"); }

  void rewrite(form::Form &form, SHVar &v) {
    switch (form.index()) {
    case form::SPECIAL: {
      auto &error = std::get<form::Special>(form);
      SHLOG_ERROR("EDN parsing error: {}", error.message);
      throw ActivationError("EDN parsing error");
    }
    case form::TOKEN:
      rewrite(std::get<token::Token>(form), v);
      break;
    case form::LIST:
      rewrite<std::list<form::FormWrapper>>(std::get<std::list<form::FormWrapper>>(form), v);
      break;
    case form::VECTOR:
      rewrite<std::vector<form::FormWrapper>>(std::get<std::vector<form::FormWrapper>>(form), v);
      break;
    case form::MAP:
      rewrite(std::get<form::FormWrapperMap>(form), v);
      break;
    case form::SET:
      rewrite<form::FormWrapperSet>(std::get<form::FormWrapperSet>(form), v);
      break;
    }
  }

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
    BOOST_FOREACH (auto &item, list) {
      vars.emplace_back(to_var(item));
    }
    return Var(vars);
  }

  OwnedVar to_var(form::FormWrapper &formWrapper) { return to_var(formWrapper.form); }

  SHMap to_var(form::FormWrapperMap &map) {
    SHMap res;
    BOOST_FOREACH (auto &item, map) {
      OwnedVar key = to_var(item.first.form);
      if (key.valueType != SHType::String)
        throw ActivationError("Map key must be a string");
      OwnedVar value = to_var(item.second.form);
      res[key] = value;
    }
    return res;
  }

  OwnedVar to_var(form::Form form) {
    switch (form.index()) {
    case form::SPECIAL: {
      auto &error = std::get<form::Special>(form);
      SHLOG_ERROR("EDN parsing error: {}", error.message);
      throw ActivationError("EDN parsing error");
    }
    case form::TOKEN:
      return to_var(std::get<token::Token>(form));
    case form::LIST:
      return to_var<std::list<form::FormWrapper>>(std::get<std::list<form::FormWrapper>>(form));
    case form::VECTOR:
      return to_var<std::vector<form::FormWrapper>>(std::get<std::vector<form::FormWrapper>>(form));
    case form::MAP: {
      SHMap m = to_var(std::get<form::FormWrapperMap>(form));
      SHVar tmp{};
      tmp.valueType = SHType::Table;
      tmp.payload.tableValue.api = &GetGlobals().TableInterface;
      tmp.payload.tableValue.opaque = &m;
      return tmp;
    }
    case form::SET:
      return to_var<form::FormWrapperSet>(std::get<form::FormWrapperSet>(form));
    }
    return Var::Empty;
  }

  bool find_symbols(token::Token &token) {
    if (token.value.index() == token::value::STRING && token.type >= token::type::SYMBOL) {
      return true;
    } else {
      return false;
    }
  }

  template <class T> void find_symbols(T &seq, bool list) {
    bool first = true;
    BOOST_FOREACH (auto &item, seq) {
      // always run find_symbols
      if (find_symbols(item) && first && list) {
        // execute shards here
        const auto &token = std::get<token::Token>(item.form);
        const auto &name = std::get<std::string>(token.value);
        auto idx = 0;
        for (auto &case_ : _cases) {
          if (case_.valueType == SHType::String) {
            auto cs = SHSTRVIEW(case_);
            if (name == cs) {
              auto v = to_var(seq);
              SHLOG_DEBUG("Found symbol: {} value: {}", name, v);
              if (_actions[idx]) {
                SHVar output = v;
                const auto state = _actions[idx].activate(_context, v, output);
                if (state == SHWireState::Continue) {
                  rewrite(seq, output);
                }
              }
            }
          }
          idx++;
        }
      }
      first = false;
    }
  }

  bool find_symbols(form::FormWrapper &formWrapper) { return find_symbols(formWrapper.form); }

  void find_symbols(form::FormWrapperMap &map) {
    BOOST_FOREACH (auto &item, map) {
      find_symbols(item.second.form);
    }
  }

  bool find_symbols(form::Form &form) {
    switch (form.index()) {
    case form::SPECIAL: {
      auto &error = std::get<form::Special>(form);
      SHLOG_ERROR("EDN parsing error: {}", error.message);
      throw ActivationError("EDN parsing error");
    }
    case form::TOKEN:
      return find_symbols(std::get<token::Token>(form));
    case form::LIST:
      find_symbols<std::list<form::FormWrapper>>(std::get<std::list<form::FormWrapper>>(form), true);
      break;
    case form::VECTOR:
      find_symbols<std::vector<form::FormWrapper>>(std::get<std::vector<form::FormWrapper>>(form), false);
      break;
    case form::MAP:
      find_symbols(std::get<form::FormWrapperMap>(form));
      break;
    case form::SET:
      find_symbols<form::FormWrapperSet>(std::get<form::FormWrapperSet>(form), false);
      break;
    }
    return false;
  }

  void find_symbols(std::list<form::Form> &forms) {
    for (auto &form : forms) {
      find_symbols(form);
    }
  }

  SHTypeInfo compose(const SHInstanceData &data) {
    SHInstanceData dataCopy = data;
    dataCopy.inputType = CoreInfo::AnySeqType;
    for (auto &action : _actions) {
      if (action)
        action.compose(dataCopy);
    }

    return data.inputType;
  }

  void warmup(SHContext *context) {
    for (auto &action : _actions) {
      if (action)
        action.warmup(context);
    }
  }

  void cleanup() {
    for (auto &action : _actions) {
      if (action)
        action.cleanup();
    }
  }

  SHVar activate(SHContext *context, const SHVar &input) {
    _context = context;
    const auto s = input.payload.stringLen > 0 ? std::string(input.payload.stringValue, input.payload.stringLen)
                                               : std::string(input.payload.stringValue);
    auto forms = read(s);
    if (_cases.size())
      find_symbols(forms);
    _output.assign(print(forms));
    return Var(_output);
  }
};

#if 0
struct Parse {
  std::vector<OwnedVar> _output;

  static SHTypesInfo inputTypes() { return CoreInfo::StringType; }
  static SHTypesInfo outputTypes() { return CoreInfo::AnySeqType; }

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

  SHMap to_var(form::FormWrapperMap &map) {
    SHMap res;
    BOOST_FOREACH (auto &item, map) {
      OwnedVar key = to_var(item.first.form);
      if (key.valueType != SHType::String)
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
      SHLOG_ERROR("EDN parsing error: {}", error.message);
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
      SHMap m = to_var(std::get<form::FormWrapperMap>(form));
      SHVar tmp{};
      tmp.valueType = SHType::Table;
      tmp.payload.tableValue.api = &GetGlobals().TableInterface;
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

  SHVar activate(SHContext *context, const SHVar &input) {
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

  static SHTypesInfo inputTypes() { return CoreInfo::AnySeqType; }
  static SHTypesInfo outputTypes() { return CoreInfo::StringType; }

  SHVar activate(SHContext *context, const SHVar &input) {
    return Var(_output);
  }
};
#endif
} // namespace edn
} // namespace shards

SHARDS_REGISTER_FN(edn) {
  using namespace shards::edn;
  REGISTER_SHARD("EDN.Uglify", Uglify);
#if 0
  REGISTER_SHARD("EDN.Parse", Parse);
  REGISTER_SHARD("EDN.Render", Render);
#endif
}