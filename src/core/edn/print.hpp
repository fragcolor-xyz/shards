/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

// Derived from https://github.com/oakes/ZachLisp

#ifndef SH_LSP_PRINT_HPP
#define SH_LSP_PRINT_HPP

#include "read.hpp"
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>

namespace shards {
namespace edn {

inline std::string to_string(double d) {
  std::ostringstream stm;
  stm << d;
  if (d == int64_t(d)) {
    // we want to add .0 always
    stm << ".0";
  }
  return stm.str();
}

struct document {
  int indent = 0;
};

inline std::string escape_str(std::string &s) {
  // return std::regex_replace(s, std::regex("\""), "\\\"");
  return s;
}

inline std::string pr_str(document &doc, token::Token &token) {
  switch (token.value.index()) {
  case token::value::BOOL:
    return std::get<bool>(token.value) ? "true" : "false";
  case token::value::CHAR:
    return std::string(1, std::get<char>(token.value));
  case token::value::LONG:
    return std::to_string(std::get<int64_t>(token.value));
  case token::value::DOUBLE:
    return to_string(std::get<double>(token.value));
  case token::value::STRING: {
    std::string s = std::get<std::string>(token.value);
    if (token.type == token::type::STRING) {
      return "\"" + escape_str(s) + "\"";
    } else if (token.type == token::type::RAW_STRING) {
      return "#\"" + escape_str(s) + "\"";
    } else {
      return s;
    }
  }
  }
  return "";
}

inline std::string pr_str(document &doc, form::Form form);

inline std::string pr_str(document &doc, form::FormWrapper formWrapper) { return pr_str(doc, formWrapper.form); }

template <class T> inline std::string pr_str(document &doc, T &list) {
  std::string s;
  BOOST_FOREACH (auto &item, list) {
    if (s.size() > 0) {
      s += " ";
    }
    s += pr_str(doc, item);
  }
  return s;
}

inline std::string pr_str(document &doc, form::FormWrapperMap &map) {
  std::string s;
  BOOST_FOREACH (auto &item, map) {
    if (s.size() > 0) {
      s += " ";
    }
    s += pr_str(doc, item.first.form) + " " + pr_str(doc, item.second.form);
  }
  return s;
}

inline std::string pr_str(document &doc, form::Form form) {
  switch (form.index()) {
  case form::SPECIAL: {
    auto error = std::get<form::Special>(form);
    return "#" + error.name + " \"" + escape_str(error.message) + "\"";
  }
  case form::TOKEN:
    return pr_str(doc, std::get<token::Token>(form));
  case form::LIST:
    return "(" + pr_str<std::list<form::FormWrapper>>(doc, std::get<std::list<form::FormWrapper>>(form)) + ")";
  case form::VECTOR:
    return "[" + pr_str<std::vector<form::FormWrapper>>(doc, std::get<std::vector<form::FormWrapper>>(form)) + "]";
  case form::MAP:
    return "{" + pr_str(doc, std::get<form::FormWrapperMap>(form)) + "}";
  case form::SET:
    return "#{" + pr_str<form::FormWrapperSet>(doc, std::get<form::FormWrapperSet>(form)) + "}";
  }
  return "";
}

inline std::string print(const std::list<form::Form> &forms, char separator = ' ') {
  std::string s;
  document doc;
  for (auto &form : forms) {
    s += pr_str(doc, form) + separator;
  }
  return s;
}

} // namespace edn
} // namespace shards

#endif
