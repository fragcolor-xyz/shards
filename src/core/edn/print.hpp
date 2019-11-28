/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

// Derived from https://github.com/oakes/ZachLisp

#ifndef CB_LSP_PRINT_HPP
#define CB_LSP_PRINT_HPP

#include "read.hpp"

namespace chainblocks {
namespace edn {

struct document {
  constexpr auto indents = 2;
  int indent = 0;
};

std::string escape_str(std::string &s) {
  // return std::regex_replace(s, std::regex("\""), "\\\"");
  return s;
}

std::string pr_str(document &doc, token::Token &token) {
  switch (token.value.index()) {
  case token::value::BOOL:
    return std::get<bool>(token.value) ? "true" : "false";
  case token::value::CHAR:
    return std::string(1, std::get<char>(token.value));
  case token::value::LONG:
    return std::to_string(std::get<int64_t>(token.value));
  case token::value::DOUBLE:
    return std::to_string(std::get<double>(token.value));
  case token::value::STRING: {
    std::string s = std::get<std::string>(token.value);
    if (token.type == token::type::STRING) {
      return "\"" + escape_str(s) + "\"";
    } else {
      return s;
    }
  }
  }
  return "";
}

std::string pr_str(document &doc, form::Form form);

std::string pr_str(document &doc, form::FormWrapper formWrapper) {
  return pr_str(doc, formWrapper.form);
}

std::string pr_str(document &doc, std::string &s) { return s; }

template <class T> std::string pr_str(document &doc, T &list) {
  std::string s;
  for (auto &item : list) {
    if (s.size() > 0) {
      s += " ";
    }
    s += pr_str(doc, item);
  }
  return s;
}

std::string pr_str(document &doc, form::FormWrapperMap &map) {
  std::string s;
  for (auto &item : map) {
    if (s.size() > 0) {
      s += " ";
    }
    s += pr_str(doc, item.first.form) + " " + pr_str(doc, item.second.form);
  }
  return s;
}

std::string pr_str(document &doc, form::Form form) {
  switch (form.index()) {
  case form::SPECIAL: {
    auto error = std::get<form::Special>(form);
    return "#" + error.name + " \"" + escape_str(error.message) + "\"";
  }
  case form::TOKEN:
    return pr_str(doc, std::get<token::Token>(form));
  case form::LIST:
    return "\n(" +
           pr_str<std::list<form::FormWrapper>>(
               doc, std::get<std::list<form::FormWrapper>>(form)) +
           ")";
  case form::VECTOR:
    return "\n[" +
           pr_str<std::vector<form::FormWrapper>>(
               doc, std::get<std::vector<form::FormWrapper>>(form)) +
           "]";
  case form::MAP:
    return "\n{" +
           pr_str(doc, *std::get<std::shared_ptr<form::FormWrapperMap>>(form)) +
           "}";
  case form::SET:
    return "\n#{" +
           pr_str<form::FormWrapperSet>(
               doc, *std::get<std::shared_ptr<form::FormWrapperSet>>(form)) +
           "}";
  }
  return "";
}

std::string print(const std::list<form::Form> &forms) {
  std::string s;
  document doc;
  for (auto &form : forms) {
    s += pr_str(doc, form) + "\n";
  }
  return s;
}

} // namespace edn
} // namespace chainblocks

#endif
