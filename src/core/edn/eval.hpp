

#ifndef CB_LSP_EVAL_HPP
#define CB_LSP_EVAL_HPP

#include "read.hpp"

namespace chainblocks {
namespace edn {
namespace eval {
class EvalException : public std::exception {
public:
  explicit EvalException(std::string errmsg, token::Token token)
      : errorMessage(errmsg) {
    errorMessage = "At line: " + std::to_string(token.line) +
                   " column: " + std::to_string(token.column) +
                   " error: " + errorMessage;
  }

  [[nodiscard]] const char *what() const noexcept override {
    return errorMessage.c_str();
  }

private:
  std::string errorMessage;
};

class Environment;

class Value {
public:
  Value(token::Token token, const std::shared_ptr<Environment> &env)
      : _token(token), _owner(env) {}

private:
  token::Token _token;
  std::weak_ptr<Environment> _owner;
};

class Environment {
public:
  void set(std::string name, std::shared_ptr<Value> value) {
    _contents[name] = value;
  }

private:
  phmap::flat_hash_map<std::string, std::shared_ptr<Value>> _contents;
  std::weak_ptr<Environment> _parent;
};

class Program {
public:
  Program() { _rootEnv = std::make_shared<Environment>(); }

  std::shared_ptr<Value> eval(form::Form ast,
                              std::shared_ptr<Environment> env) {
    while (1) {
      auto idx = ast.index();
      if (idx == form::LIST) {
        auto list = std::get<std::list<form::FormWrapper>>(ast);
        // get first and pop it
        auto head = list.front().form;
        list.pop_front();

        switch (head.index()) {
        case form::TOKEN: {
          auto token = std::get<token::Token>(head);
          if (token.type == token::type::SYMBOL) {
            auto &value = std::get<std::string>(token.value);
            if (value == "do") {
              if (list.size() == 0) {
                throw EvalException("do requires at least one argument", token);
              }

              // take the last for tail calling
              auto ast = list.back().form;
              list.pop_back();

              for (auto &item : list) {
                eval(item.form, env);
              }

              continue;
            } else if (value == "def!") {
              if (list.size() != 2) {
                throw EvalException("def! requires two arguments", token);
              }

              auto fname = list.front().form;
              auto fbody = list.back().form;
              if (fname.index() != form::TOKEN) {
                throw EvalException(
                    "def! first argument should be a valid symbol name", token);
              }
              auto tname = std::get<token::Token>(fname);
              if (tname.type != token::type::SYMBOL) {
                throw EvalException(
                    "def! first argument should be a valid symbol name", tname);
              }

              auto name = std::get<std::string>(tname.value);
              auto sym = eval(fbody, env);
              env->set(name, sym);
              return sym;
            }
          }

          break;
        }
        }
      } else {
        switch (idx) {
        case form::TOKEN: {
          return std::make_shared<Value>(std::get<token::Token>(ast), env);
        }
        }
      }
    }
  }

  std::shared_ptr<Value> eval(const std::string &code) {
    auto forms = read(code);
    auto last = forms.back();
    forms.pop_back();
    for (auto &form : forms) {
      eval(form, _rootEnv);
    }
    return eval(last, _rootEnv);
  }

private:
  std::string _directory;
  std::shared_ptr<Environment> _rootEnv;
};
} // namespace eval
} // namespace edn
} // namespace chainblocks

#endif
