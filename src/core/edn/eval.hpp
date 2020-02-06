

#ifndef CB_LSP_EVAL_HPP
#define CB_LSP_EVAL_HPP

#include "../runtime.hpp"
#include "print.hpp"
#include "read.hpp"

namespace chainblocks {
namespace edn {
namespace eval {
class EvalException : public std::exception {
public:
  explicit EvalException(std::string errmsg, token::Token token)
      : errorMessage(errmsg) {
    errorMessage =
        "Script evaluation error!\nAt line: " + std::to_string(token.line) +
        "\ncolumn: " + std::to_string(token.column) +
        "\nerror: " + errorMessage;
  }

  [[nodiscard]] const char *what() const noexcept override {
    return errorMessage.c_str();
  }

private:
  std::string errorMessage;
};

class Environment;

class ValueBase {
public:
  ValueBase(const token::Token &token, const std::shared_ptr<Environment> &env)
      : _token(token), _owner(env) {}

  virtual std::string pr_str(document &doc) {
    return ::chainblocks::edn::pr_str(doc, _token);
  }

private:
  token::Token _token;
  std::weak_ptr<Environment> _owner;
};

class CBVarValue : public ValueBase {
public:
  CBVarValue(const token::Token &token, const std::shared_ptr<Environment> &env)
      : ValueBase(token, env), _var({}) {}

  const CBVar &value() const { return _var; }

protected:
  CBVar _var;
};

class NilValue : public CBVarValue {
public:
  NilValue(const token::Token &token, const std::shared_ptr<Environment> &env)
      : CBVarValue(token, env) {}
};

class IntValue : public CBVarValue {
public:
  IntValue(int64_t value, const token::Token &token,
           const std::shared_ptr<Environment> &env)
      : CBVarValue(token, env) {
    _var.valueType = Int;
    _var.payload.intValue = value;
  }
};

class FloatValue : public CBVarValue {
public:
  FloatValue(double value, const token::Token &token,
             const std::shared_ptr<Environment> &env)
      : CBVarValue(token, env) {
    _var.valueType = Float;
    _var.payload.floatValue = value;
  }
};

class StringValue : public CBVarValue {
public:
  StringValue(std::string value, const token::Token &token,
              const std::shared_ptr<Environment> &env)
      : CBVarValue(token, env), _storage(value) {
    _var.valueType = String;
    _var.payload.stringValue = _storage.c_str();
  }

private:
  std::string _storage;
};

class BoolValue : public CBVarValue {
public:
  BoolValue(bool value, const token::Token &token,
            const std::shared_ptr<Environment> &env)
      : CBVarValue(token, env) {
    _var.valueType = Bool;
    _var.payload.boolValue = value;
  }
};

class Lambda : public ValueBase {
public:
  Lambda(const std::vector<std::string> &argNames,
         const form::FormWrapper &body, const token::Token &token,
         const std::shared_ptr<Environment> &env)
      : ValueBase(token, env), _argNames(argNames), _body(body) {}

  const form::Form &body() const { return _body.form; }
  const std::vector<std::string> &args() { return _argNames; }

private:
  std::vector<std::string> _argNames;
  form::FormWrapper _body;
};

class Node : public ValueBase {
public:
  Node(const token::Token &token, const std::shared_ptr<Environment> &env)
      : ValueBase(token, env) {
    _node = std::make_shared<CBNode>();
  }

private:
  std::shared_ptr<CBNode> _node;
};

using Value = std::variant<CBVarValue, Lambda, Node>;

class Environment {
public:
  Environment() {}
  Environment(std::shared_ptr<Environment> parent) : _parent(parent) {}

  void set(const std::string &name, Value value, bool deep = false) {
    if (deep && _parent) {
      _parent->set(name, value, deep);
    } else {
      _contents.insert(std::pair<std::string, Value>(name, value));
    }
  }

  Value *find(const std::string &name) {
    auto it = _contents.find(name);
    if (it != _contents.end()) {
      return &it->second;
    }
    if (_parent)
      return _parent->find(name);
    else
      return nullptr;
  }

private:
  std::unordered_map<std::string, Value> _contents;
  std::shared_ptr<Environment> _parent;
};

class Program {
public:
  Program() { _rootEnv = std::make_shared<Environment>(); }

  Value eval(form::Form ast, std::shared_ptr<Environment> env) {
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
            } else if (value == "def") {
              if (list.size() != 2) {
                throw EvalException("def! requires two arguments", token);
              }

              auto fname = list.front().form;
              auto fbody = list.back().form;
              if (fname.index() != form::TOKEN) {
                throw EvalException(
                    "def first argument should be a valid symbol name", token);
              }
              auto tname = std::get<token::Token>(fname);
              if (tname.type != token::type::SYMBOL) {
                throw EvalException(
                    "def first argument should be a valid symbol name", tname);
              }

              auto name = std::get<std::string>(tname.value);
              auto sym = eval(fbody, env);
              // set "deep" into global root env
              env->set(name, sym, true);
              return sym;
            } else if (value == "fn") {
              // Nice to have - multi-arity function
              if (list.size() != 2) {
                throw EvalException("fn requires two arguments", token);
              }

              auto fbindings = list.front().form;
              auto fbody = list.back().form;
              if (fbindings.index() != form::VECTOR) {
                throw EvalException("fn first arguments should be a vector",
                                    token);
              }

              std::vector<std::string> args;
              auto bindings =
                  std::get<std::vector<form::FormWrapper>>(fbindings);
              for (auto &binding : bindings) {
                auto &fbind = binding.form;
                if (fbind.index() != form::TOKEN) {
                  throw EvalException(
                      "fn arguments have to be a valid symbol name", token);
                }
                auto &tbind = std::get<token::Token>(fbind);
                if (tbind.type != token::type::SYMBOL) {
                  throw EvalException(
                      "fn arguments have to be a valid symbol name", tbind);
                }
                auto &name = std::get<std::string>(tbind.value);
                args.push_back(name);
              }

              return Lambda(args, fbody, token, env);
            } else if (value == "if") {
              if (list.size() < 2 || list.size() > 3) {
                throw EvalException("if expects between 1 or 2 arguments",
                                    token);
              }

              auto &pred = list.front().form;
              auto res = eval(pred, env);
              list.pop_front();
              if (res.index() == 0) {
                // is cbvarvalue
                auto &var = std::get<CBVarValue>(res);
                if (var.value().valueType == Bool &&
                    var.value().payload.boolValue) {
                  // is true
                  ast = list.front().form;
                  continue; // tailcall
                }
              }

              if (list.size() == 2) {
                ast = list.back().form;
                continue; // tailcall
              }

              return NilValue(token, env);
            } else if (value == "Node") {
              return Node(token, env);
            } else {
              // seek the env
              auto sym = env->find(value);
              if (sym) {
                if (sym->index() == 1) { // lambda
                  auto &lmbd = std::get<Lambda>(*sym);
                  ast = lmbd.body();
                  // create a env with args
                  auto subenv = std::make_shared<Environment>(env);
                  auto args = lmbd.args();
                  for (auto &arg : args) {
                    if (list.size() == 0) {
                      throw EvalException(
                          "Not enough arguments for call: " + value, token);
                    }
                    auto argAst = list.front().form;
                    list.pop_front();
                    // eval the arg and add it to the local env
                    auto argVal = eval(argAst, env);
                    subenv->set(arg, argVal);
                  }
                  env = subenv;
                  continue; // tailcall
                }
              } else {
                throw EvalException("Symbol not found: " + value, token);
              }
            }
          }
          break;
        }
        }
      } else {
        switch (idx) {
        case form::TOKEN: {
          auto token = std::get<token::Token>(ast);
          if (token.type == token::type::SYMBOL) {
            if (token.value.index() == token::value::STRING) {
              auto &value = std::get<std::string>(token.value);
              if (value == "nil") {
                return NilValue(token, env);
              } else {
                auto sym = env->find(value);
                if (sym) {
                  return *sym;
                } else {
                  throw EvalException("Symbol not found: " + value, token);
                }
              }
            } else if (token.value.index() == token::value::BOOL) {
              return BoolValue(std::get<bool>(token.value), token, env);
            }
          } else if (token.type == token::type::NUMBER) {
            if (token.value.index() == token::value::LONG) {
              return IntValue(std::get<int64_t>(token.value), token, env);
            } else {
              return FloatValue(std::get<double>(token.value), token, env);
            }
          } else if (token.type == token::type::HEX) {
            return IntValue(std::get<int64_t>(token.value), token, env);
          } else if (token.type == token::type::STRING) {
            return StringValue(std::get<std::string>(token.value), token, env);
          }
        }
        }
      }
    }
  }

  Value eval(const std::string &code) {
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
