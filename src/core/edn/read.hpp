// Derived from https://github.com/oakes/ZachLisp

#ifndef CB_LSP_READ_HPP
#define CB_LSP_READ_HPP

#include <boost/container/map.hpp>
#include <boost/container/set.hpp>
#include <boost/foreach.hpp>
#include <cstdint>
#include <list>
#include <map>
#include <optional>
#include <regex>
#include <set>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

namespace chainblocks {
namespace edn {

namespace token {

namespace type {

enum Type { WHITESPACE, SPECIAL_CHARS, SPECIAL_CHAR, STRING, RAW_STRING, COMMENT, HEX, DOUBLE, INT, SYMBOL };

}

namespace value {

using Value = std::variant<bool, char, int64_t, double, std::string>;

enum Type { BOOL, CHAR, LONG, DOUBLE, STRING };

} // namespace value

const std::regex REGEX("([\\s,]+)|"                                // type::WHITESPACE
                       "(~@|#\\{)|"                                // type::SPECIAL_CHARS
                       "([\\[\\]{}()\'`~^@])|"                     // type::SPECIAL_CHAR
                       "(\"(?:\\\\.|[^\\\\\"])*\"?)|"              // type::STRING
                       "(#\"(?:\\\\.|[^\\\\\"])*\"?)|"             // type::RAW_STRING
                       "(;.*)|"                                    // type::COMMENT
                       "(0[xX][0-9a-fA-F]+)|"                      // type::HEX
                       "([-+]?[0-9]*\\.[0-9]+([eE][-+]?[0-9]+)?)|" // type::DOUBLE
                       "([-+]?\\d+)|"                              // type::INT
                       "([^\\s\\[\\]{}(\'\"`,;)]+)"                // type::SYMBOL
);

struct Token {
  value::Value value;
  type::Type type;
  int line;
  int column;

  Token(value::Value v, type::Type t, int l, int c) : value(v), type(t), line(l), column(c) {}

  bool operator==(const Token &t) const { return (value == t.value) && (type == t.type); }

  bool operator<(const Token &t) const { return (value < t.value) || (type < t.type); }
};

} // namespace token

namespace form {

struct Special {
  std::string name;
  std::string message;
  std::optional<token::Token> token;

  Special(std::string n, std::string m, std::optional<token::Token> t) : name(n), message(m), token(t) {}

  bool operator==(const Special &re) const { return (!message.compare(re.message)) && (token == re.token); }

  bool operator<(const Special &re) const { return (message < re.message) || (token < re.token); }
};

struct FormWrapper;

using FormWrapperMap = boost::container::map<FormWrapper, FormWrapper>;
using FormWrapperSet = boost::container::set<FormWrapper>;

using Form =
    std::variant<Special, token::Token, std::list<FormWrapper>, std::vector<FormWrapper>, FormWrapperMap, FormWrapperSet>;

enum Type { SPECIAL, TOKEN, LIST, VECTOR, MAP, SET };

std::size_t hash(const FormWrapper &fw);
bool equals(const FormWrapper &fw1, const FormWrapper &fw2);
bool isLess(const FormWrapper &fw1, const FormWrapper &fw2);

} // namespace form

// see: https://stackoverflow.com/a/38140932

inline void hash_combine(std::size_t &seed) {}

template <typename T, typename... Rest> inline void hash_combine(std::size_t &seed, const T &v, Rest... rest) {
  std::hash<T> hasher;
  seed ^= hasher(v) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
  hash_combine(seed, rest...);
}

} // namespace edn
} // namespace chainblocks

namespace std {

template <> struct hash<chainblocks::edn::token::Token> {
  size_t operator()(const chainblocks::edn::token::Token &x) const {
    return std::hash<chainblocks::edn::token::value::Value>()(x.value);
  }
};

template <> struct hash<chainblocks::edn::form::Special> {
  size_t operator()(const chainblocks::edn::form::Special &x) const { return std::hash<std::string>()(x.message); }
};

template <> struct hash<chainblocks::edn::form::FormWrapper> {
  size_t operator()(const chainblocks::edn::form::FormWrapper &x) const { return chainblocks::edn::form::hash(x); }
};

} // namespace std

namespace chainblocks {
namespace edn {

namespace token {

inline value::Value parse(std::string value, type::Type type) {
  switch (type) {
  case type::SPECIAL_CHAR:
    return value[0];
  case type::HEX:
    return (int64_t)std::stoll(value, nullptr, 16);
  case type::INT:
    return (int64_t)std::stoll(value);
  case type::DOUBLE:
    return std::stod(value);
  case type::SYMBOL:
    if (value == "true") {
      return true;
    } else if (value == "false") {
      return false;
    }
  default:
    return value;
  }
}

inline std::list<Token> tokenize(std::string input) {
  std::sregex_iterator begin(input.begin(), input.end(), REGEX);
  std::sregex_iterator end;

  std::list<Token> tokens;

  int line = 1;

  for (auto it = begin; it != end; ++it) {
    std::smatch match = *it;
    for (size_t i = 1; i < match.size(); ++i) {
      if (!match[i].str().empty()) {
        std::string value_str = match.str();
        type::Type type = static_cast<type::Type>(i - 1);
        value::Value value = parse(value_str, type);
        int column = match.position() + 1;
        tokens.push_back(Token{value, type, line, column});
        line += std::count(value_str.begin(), value_str.end(), '\n');
        break;
      }
    }
  }

  return tokens;
}

} // namespace token

namespace form {

struct FormWrapper {
  Form form;

  FormWrapper(Form f) : form(f) {}

  bool operator==(const FormWrapper &fw) const { return equals(*this, fw); }
  bool operator<(const FormWrapper &fw) const { return isLess(*this, fw); }
};

template <class T> inline std::size_t hash(T list) {
  std::size_t ret = 0;
  for (auto item : list) {
    hash_combine(ret, item);
  }
  return ret;
}

inline std::size_t hash(FormWrapperSet set) {
  std::vector<std::size_t> hashes;
  BOOST_FOREACH (auto &item, set) {
    hashes.push_back(hash(item));
  }

  std::sort(hashes.begin(), hashes.end());

  std::size_t ret = 0;
  for (auto hash : hashes) {
    hash_combine(ret, hash);
  }
  return ret;
}

inline std::size_t hash(FormWrapperMap map) {
  std::vector<std::size_t> hashes;
  BOOST_FOREACH (auto &item, map) {
    std::size_t hash = 0;
    hash_combine(hash, item.first, item.second);
    hashes.push_back(hash);
  }

  std::sort(hashes.begin(), hashes.end());

  std::size_t ret = 0;
  for (auto hash : hashes) {
    hash_combine(ret, hash);
  }
  return ret;
}

inline std::size_t hash(const FormWrapper &fw) {
  switch (fw.form.index()) {
  case SPECIAL:
    return std::hash<form::Special>()(std::get<form::Special>(fw.form));
  case TOKEN:
    return std::hash<token::Token>()(std::get<token::Token>(fw.form));
  case LIST:
    return hash<std::list<FormWrapper>>(std::get<std::list<FormWrapper>>(fw.form));
  case VECTOR:
    return hash<std::vector<FormWrapper>>(std::get<std::vector<FormWrapper>>(fw.form));
  case MAP:
    return hash(std::get<FormWrapperMap>(fw.form));
  case SET:
    return hash(std::get<FormWrapperSet>(fw.form));
  }
  return 0;
}

inline bool equals(const FormWrapper &fw1, const FormWrapper &fw2) {
  if (fw1.form.index() != fw2.form.index())
    return false;

  switch (fw1.form.index()) {
  case SPECIAL: {
    auto &a = std::get<form::Special>(fw1.form);
    auto &b = std::get<form::Special>(fw2.form);
    return a == b;
  }
  case TOKEN: {
    auto &a = std::get<token::Token>(fw1.form);
    auto &b = std::get<token::Token>(fw2.form);
    return a == b;
  }
  case LIST: {
    auto &a = std::get<std::list<FormWrapper>>(fw1.form);
    auto &b = std::get<std::list<FormWrapper>>(fw2.form);
    return a == b;
  }
  case VECTOR: {
    auto &a = std::get<std::vector<FormWrapper>>(fw1.form);
    auto &b = std::get<std::vector<FormWrapper>>(fw2.form);
    return a == b;
  }
  case MAP: {
    auto &a = std::get<FormWrapperMap>(fw1.form);
    auto &b = std::get<FormWrapperMap>(fw2.form);
    return a == b;
  }
  case SET: {
    auto &a = std::get<FormWrapperSet>(fw1.form);
    auto &b = std::get<FormWrapperSet>(fw2.form);
    return a == b;
  }
  default:
    return false;
  }
}

inline bool isLess(const FormWrapper &fw1, const FormWrapper &fw2) {
  if (fw1.form.index() < fw2.form.index())
    return true;

  switch (fw1.form.index()) {
  case SPECIAL: {
    auto &a = std::get<form::Special>(fw1.form);
    auto &b = std::get<form::Special>(fw2.form);
    return a < b;
  }
  case TOKEN: {
    auto &a = std::get<token::Token>(fw1.form);
    auto &b = std::get<token::Token>(fw2.form);
    return a < b;
  }
  case LIST: {
    auto &a = std::get<std::list<FormWrapper>>(fw1.form);
    auto &b = std::get<std::list<FormWrapper>>(fw2.form);
    return a < b;
  }
  case VECTOR: {
    auto &a = std::get<std::vector<FormWrapper>>(fw1.form);
    auto &b = std::get<std::vector<FormWrapper>>(fw2.form);
    return a < b;
  }
  case MAP: {
    auto &a = std::get<FormWrapperMap>(fw1.form);
    auto &b = std::get<FormWrapperMap>(fw2.form);
    return a < b;
  }
  case SET: {
    auto &a = std::get<FormWrapperSet>(fw1.form);
    auto &b = std::get<FormWrapperSet>(fw2.form);
    return a < b;
  }
  default:
    return true;
  }
}

} // namespace form

const std::unordered_map<std::variant<char, std::string>, std::string> SYMBOL_TO_NAME = {
    {'\'', "quote"}, {'`', "quasiquote"}, {'~', "unquote"}, {'@', "deref"}, {'^', "with-meta"}, {"~@", "splice-unquote"}};

const std::unordered_map<std::variant<char, std::string>, form::Type> DELIMITER_TO_TYPE = {
    {'(', form::LIST},
    {'[', form::VECTOR},
    {'{', form::MAP},
    {"#{", form::SET},
};

const std::unordered_map<form::Type, char> TYPE_TO_DELIMITER = {
    {form::LIST, ')'}, {form::VECTOR, ']'}, {form::MAP, '}'}, {form::SET, '}'}};

inline std::pair<form::Form, std::list<token::Token>::const_iterator> read_form(const std::list<token::Token> *tokens,
                                                                                std::list<token::Token>::const_iterator it);
inline std::optional<std::pair<form::Form, std::list<token::Token>::const_iterator>>
read_useful_form(const std::list<token::Token> *tokens, std::list<token::Token>::const_iterator it);
inline std::optional<std::pair<token::Token, std::list<token::Token>::const_iterator>>
read_useful_token(const std::list<token::Token> *tokens, std::list<token::Token>::const_iterator it);

inline form::Form list_to_vector(const std::list<form::FormWrapper> list) {
  return std::vector<form::FormWrapper>{std::make_move_iterator(std::begin(list)), std::make_move_iterator(std::end(list))};
}

inline form::Form list_to_map(const std::list<form::FormWrapper> list) {
  form::FormWrapperMap m;
  form::FormWrapperMap::const_iterator map_it = m.begin();
  std::list<form::FormWrapper>::const_iterator list_it = list.begin();
  while (list_it != list.end()) {
    auto key = *list_it;
    ++list_it;
    if (list_it == list.end()) {
      return form::Special{"ReaderError", "Map must contain even number of forms", std::nullopt};
    } else {
      auto val = *list_it;
      ++list_it;
      m.insert(map_it, std::pair(key, val));
    }
  }
  return m;
}

inline form::Form list_to_set(const std::list<form::FormWrapper> list) {
  form::FormWrapperSet s;
  form::FormWrapperSet::const_iterator it = s.begin();
  for (auto item : list) {
    s.insert(it, item);
  }
  return s;
}

inline std::pair<form::Form, std::list<token::Token>::const_iterator>
read_coll(const std::list<token::Token> *tokens, std::list<token::Token>::const_iterator it, form::Type form_type) {
  char end_delimiter = TYPE_TO_DELIMITER.at(form_type);
  std::list<form::FormWrapper> forms;
  while (auto ret_opt = read_useful_token(tokens, it)) {
    auto ret = ret_opt.value();
    auto token = ret.first;
    it = ret.second;
    if (token.type == token::type::SPECIAL_CHAR) {
      char c = std::get<char>(token.value);
      if (c == end_delimiter) {
        switch (form_type) {
        case form::VECTOR:
          return std::make_pair(list_to_vector(forms), ++it);
        case form::MAP:
          return std::make_pair(list_to_map(forms), ++it);
        case form::SET:
          return std::make_pair(list_to_set(forms), ++it);
        default:
          return std::make_pair(forms, ++it);
        }
      } else {
        switch (c) {
        case ')':
        case ']':
        case '}':
          return std::make_pair(form::Special{"ReaderError", "Unmatched delimiter: " + std::string(1, c), token}, tokens->end());
        }
      }
    }
    auto ret2 = read_form(tokens, it);
    forms.push_back(form::FormWrapper{ret2.first});
    it = ret2.second;
  }
  return std::make_pair(form::Special{"ReaderError", "EOF: no " + std::string(1, end_delimiter) + " found", std::nullopt},
                        tokens->end());
}

inline std::pair<form::Form, std::list<token::Token>::const_iterator>
expand_quoted_form(const std::list<token::Token> *tokens, std::list<token::Token>::const_iterator it, token::Token token) {
  if (auto ret_opt = read_useful_form(tokens, it)) {
    auto ret = ret_opt.value();
    std::list<form::FormWrapper> list{form::FormWrapper{token}, ret.first};
    return std::make_pair(list, ret.second);
  } else {
    return std::make_pair(form::Special{"ReaderError", "EOF: Nothing found after quote", token}, tokens->end());
  }
}

inline std::pair<form::Form, std::list<token::Token>::const_iterator>
expand_meta_quoted_form(const std::list<token::Token> *tokens, std::list<token::Token>::const_iterator it, token::Token token) {
  if (auto ret_opt = read_useful_form(tokens, it)) {
    auto ret = ret_opt.value();
    if (auto ret_opt2 = read_useful_form(tokens, ret.second)) {
      auto ret2 = ret_opt2.value();
      std::list<form::FormWrapper> list{form::FormWrapper{token}, ret2.first, ret.first};
      return std::make_pair(list, ret2.second);
    } else {
      return std::make_pair(form::Special{"ReaderError", "EOF: Nothing found after metadata", token}, tokens->end());
    }
  } else {
    return std::make_pair(form::Special{"ReaderError", "EOF: Nothing found after ^", token}, tokens->end());
  }
}

inline char unescape(char c) {
  switch (c) {
  case '\\':
    return '\\';
  case 'n':
    return '\n';
  case '"':
    return '"';
  default:
    return c;
  }
}

inline std::string &unescape(std::string &in) {
  // in will have double-quotes at either end, so move the iterators in
  for (auto it = in.begin(), end = in.end(); it != end; ++it) {
    const char c = *it;
    if (c == '\\') {
      if (it != end) {
        *it = unescape(c);
      }
    }
  }
  return in;
}

inline std::pair<form::Form, std::list<token::Token>::const_iterator> read_form(const std::list<token::Token> *tokens,
                                                                                std::list<token::Token>::const_iterator it) {
  auto token = *it;
  switch (token.type) {
  case token::type::SPECIAL_CHARS: {
    std::string s = std::get<std::string>(token.value);
    if (s == "#{") {
      return read_coll(tokens, ++it, DELIMITER_TO_TYPE.at(s));
    } else if (s == "~@") {
      return expand_quoted_form(tokens, ++it, token::Token{SYMBOL_TO_NAME.at(s), token::type::SYMBOL, token.line, token.column});
    }
    break;
  }
  case token::type::SPECIAL_CHAR: {
    char c = std::get<char>(token.value);
    switch (c) {
    case '(':
    case '[':
    case '{':
      return read_coll(tokens, ++it, DELIMITER_TO_TYPE.at(c));
    case ')':
    case ']':
    case '}':
      return std::make_pair(form::Special{"ReaderError", "Unmatched delimiter: " + std::string(1, c), token}, tokens->end());
    case '\'':
    case '`':
    case '~':
    case '@':
      return expand_quoted_form(tokens, ++it, token::Token{SYMBOL_TO_NAME.at(c), token::type::SYMBOL, token.line, token.column});
    case '^':
      return expand_meta_quoted_form(tokens, ++it,
                                     token::Token{SYMBOL_TO_NAME.at(c), token::type::SYMBOL, token.line, token.column});
    }
    break;
  }
  case token::type::STRING: {
    std::string s = std::get<std::string>(token.value);
    if (s.size() < 2 || s.back() != '"') {
      return std::make_pair(form::Special{"ReaderError", "EOF: unbalanced quote", token}, tokens->end());
    } else {
      s = s.substr(1, s.size() - 2);
      token.value = unescape(s);
    }
    break;
  }
  case token::type::RAW_STRING: {
    std::string s = std::get<std::string>(token.value);
    if (s.size() < 3 || s.back() != '"') {
      return std::make_pair(form::Special{"ReaderError", "EOF: unbalanced quote", token}, tokens->end());
    } else {
      token.value = s.substr(2, s.size() - 3);
    }
    break;
  }
  default:
    break;
  }
  return std::make_pair(token, ++it);
}

inline std::optional<std::pair<token::Token, std::list<token::Token>::const_iterator>>
read_useful_token(const std::list<token::Token> *tokens, std::list<token::Token>::const_iterator it) {
  while (it != tokens->end()) {
    auto token = *it;
    switch (token.type) {
    case token::type::WHITESPACE:
    case token::type::COMMENT:
      ++it;
      break;
    default:
      return std::make_pair(token, it);
    }
  }
  return std::nullopt;
}

inline std::optional<std::pair<form::Form, std::list<token::Token>::const_iterator>>
read_useful_form(const std::list<token::Token> *tokens, std::list<token::Token>::const_iterator it) {
  if (auto ret_opt = read_useful_token(tokens, it)) {
    auto ret = ret_opt.value();
    return read_form(tokens, ret.second);
  } else {
    return std::nullopt;
  }
}

inline std::list<form::Form> read_forms(const std::list<token::Token> *tokens) {
  std::list<form::Form> forms;
  std::list<token::Token>::const_iterator it = tokens->begin();
  while (auto ret_opt = read_useful_form(tokens, it)) {
    auto ret = ret_opt.value();
    forms.push_back(ret.first);
    it = ret.second;
  }
  return forms;
}

inline std::list<form::Form> read(const std::string input) {
  auto tokens = token::tokenize(input);
  auto forms = read_forms(&tokens);
  return forms;
}

} // namespace edn
} // namespace chainblocks

#endif
