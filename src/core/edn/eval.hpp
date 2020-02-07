

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
  explicit EvalException(std::string errmsg, int line) : errorMessage(errmsg) {
    errorMessage = "\nScript evaluation error!\nline: " + std::to_string(line) +
                   "\nerror: " + errorMessage;
  }

  [[nodiscard]] const char *what() const noexcept override {
    return errorMessage.c_str();
  }

private:
  std::string errorMessage;
};

namespace value {
enum types { Var, Lambda, Node, Form, BuiltIn };
}

class Environment;
class CBVarValue;
class Lambda;
class Node;
class BuiltIn;
using Value = std::variant<CBVarValue, Lambda, Node, form::Form,
                           std::shared_ptr<BuiltIn>>;

class ValueBase {
public:
  ValueBase(const token::Token &token, const std::shared_ptr<Environment> &env)
      : _token(token), _owner(env) {}

  virtual std::string pr_str(document &doc) {
    return ::chainblocks::edn::pr_str(doc, _token);
  }

protected:
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

class BuiltIn {
public:
  virtual Value eval(form::Form ast, std::shared_ptr<Environment> env,
                     int *line) {
    assert(false);
  }
};

class First : public BuiltIn {
public:
  virtual ~First() {}
  Value eval(form::Form ast, std::shared_ptr<Environment> env,
             int *line) override {
    if (ast.index() == form::LIST) {
      auto list = std::get<std::list<form::FormWrapper>>(ast);
      if (list.size() == 0) {
        throw EvalException("first, did not expect an empty list", *line);
      }
      return list.front().form;
    } else if (ast.index() == form::VECTOR) {
      auto vec = std::get<std::vector<form::FormWrapper>>(ast);
      if (vec.size() == 0) {
        throw EvalException("first, did not expect an empty vector", *line);
      }
      return vec[0].form;
    } else {
      throw EvalException("first, list or vector expected", *line);
    }
  }
};

class Environment {
public:
  Environment() { set("first", std::make_shared<First>()); }
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

  Value eval(form::Form ast, std::shared_ptr<Environment> env, int *line) {
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
            if (token.value.index() == token::value::STRING) {
              auto &value = std::get<std::string>(token.value);
              if (value == "do") {
                if (list.size() == 0) {
                  throw EvalException("do requires at least one argument",
                                      token.line);
                }

                // take the last for tail calling
                auto next = list.back().form;
                list.pop_back();

                for (auto &item : list) {
                  *line = token.line;
                  eval(item.form, env, line);
                }

                ast = next;
                *line = token.line;
                continue;
              } else if (value == "def") {
                if (list.size() != 2) {
                  throw EvalException("def requires two arguments", token.line);
                }

                auto fname = list.front().form;
                auto fbody = list.back().form;
                if (fname.index() != form::TOKEN) {
                  throw EvalException(
                      "def first argument should be a valid symbol name",
                      token.line);
                }
                auto tname = std::get<token::Token>(fname);
                if (tname.type != token::type::SYMBOL) {
                  throw EvalException(
                      "def first argument should be a valid symbol name",
                      tname.line);
                }

                auto name = std::get<std::string>(tname.value);
                *line = token.line;
                auto sym = eval(fbody, env, line);
                // set "deep" into global root env
                env->set(name, sym, true);
                return sym;
              } else if (value == "fn") {
                // Nice to have - multi-arity function
                if (list.size() != 2) {
                  throw EvalException("fn requires two arguments", token.line);
                }

                auto fbindings = list.front().form;
                auto fbody = list.back().form;
                if (fbindings.index() != form::VECTOR) {
                  throw EvalException("fn first arguments should be a vector",
                                      token.line);
                }

                std::vector<std::string> args;
                auto bindings =
                    std::get<std::vector<form::FormWrapper>>(fbindings);
                for (auto &binding : bindings) {
                  auto &fbind = binding.form;
                  if (fbind.index() != form::TOKEN) {
                    throw EvalException(
                        "fn arguments have to be a valid symbol name",
                        token.line);
                  }
                  auto &tbind = std::get<token::Token>(fbind);
                  if (tbind.type != token::type::SYMBOL) {
                    throw EvalException(
                        "fn arguments have to be a valid symbol name",
                        tbind.line);
                  }
                  auto &name = std::get<std::string>(tbind.value);
                  args.push_back(name);
                }

                return Lambda(args, fbody, token, env);
              } else if (value == "if") {
                if (list.size() < 2 || list.size() > 3) {
                  throw EvalException("if expects between 1 or 2 arguments",
                                      token.line);
                }

                auto &pred = list.front().form;
                *line = token.line;
                auto res = eval(pred, env, line);
                list.pop_front();
                if (res.index() == value::types::Var) {
                  // is cbvarvalue
                  auto &var = std::get<CBVarValue>(res);
                  if (var.value().valueType == Bool &&
                      var.value().payload.boolValue) {
                    // is true
                    ast = list.front().form;
                    *line = token.line;
                    continue; // tailcall
                  }
                }

                if (list.size() == 2) {
                  ast = list.back().form;
                  *line = token.line;
                  continue; // tailcall
                }

                return NilValue(token, env);
              } else if (value == "let") {
                if (list.size() != 2) {
                  throw EvalException("let requires two arguments", token.line);
                }

                auto fbinds = list.front().form;
                auto fexprs = list.back().form;

                if (fbinds.index() != form::VECTOR) {
                  throw EvalException("let bindings should be a vector",
                                      token.line);
                }
                auto binds = std::get<std::vector<form::FormWrapper>>(fbinds);

                if ((binds.size() % 2) != 0) {
                  throw EvalException("let expected bindings in pairs",
                                      token.line);
                }

                // create a env with args
                auto subenv = std::make_shared<Environment>(env);
                for (size_t i = 0; i < binds.size(); i += 2) {
                  auto fbname = binds[i].form;
                  auto fbexpr = binds[i + 1].form;
                  if (fbname.index() != form::TOKEN) {
                    throw EvalException("let expected a token as first in the "
                                        "pair of a binding",
                                        token.line);
                  }
                  auto tbname = std::get<token::Token>(fbname);
                  if (tbname.type != token::type::SYMBOL) {
                    throw EvalException("symbol expected", tbname.line);
                  }
                  subenv->set(std::get<std::string>(tbname.value),
                              eval(fbexpr, env, line));
                }
                ast = fexprs;
                env = subenv;
                *line = token.line;
                continue; // tailcall
              } else if (value == "quote") {
                if (list.size() != 1) {
                  throw EvalException("quote expected 1 argument", token.line);
                }
                return list.back().form;
              } else if (value == "quasiquote") {
                // https://docs.racket-lang.org/reference/quasiquote.html
                if (list.size() != 1) {
                  throw EvalException("quasiquote expected 1 argument",
                                      token.line);
                }
                throw EvalException("quasiquote not implemented yet!",
                                    token.line);
              } else if (value == "Node") {
                return Node(token, env);
              } else {
                // seek the env
                auto sym = env->find(value);
                if (sym) {
                  if (sym->index() == value::types::BuiltIn) {
                    auto &bin = std::get<std::shared_ptr<BuiltIn>>(*sym);
		    // todo eval args actually!
                    return bin->eval(list, env, line); // tailcall
                  } else if (sym->index() == value::types::Lambda) {
                    auto &lmbd = std::get<Lambda>(*sym);
                    ast = lmbd.body();
                    // create a env with args
                    auto subenv = std::make_shared<Environment>(env);
                    auto args = lmbd.args();
                    for (auto &arg : args) {
                      if (list.size() == 0) {
                        throw EvalException("Not enough arguments for call: " +
                                                value,
                                            token.line);
                      }
                      auto argAst = list.front().form;
                      list.pop_front();
                      // eval the arg and add it to the local env
                      *line = token.line;
                      auto argVal = eval(argAst, env, line);
                      subenv->set(arg, argVal);
                    }
                    env = subenv;
                    *line = token.line;
                    continue; // tailcall
                  }
                } else {
                  throw EvalException("Symbol not found: " + value, token.line);
                }
              }
            } else {
              throw EvalException("Symbol expected.", token.line);
            }
          }
        } break;
        default:
          throw EvalException("Syntax error (line is approximate).", *line);
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
                  throw EvalException("Symbol not found: " + value, token.line);
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
        } break;
        default: {
          return ast;
        } break;
        }
      }
    }
  }

  Value eval(const std::string &code) {
    auto forms = read(code);
    auto last = forms.back();
    int line = 0;
    for (auto &form : forms) {
      eval(form, _rootEnv, &line);
    }
    return eval(last, _rootEnv, &line);
  }

private:
  std::string _directory;
  std::shared_ptr<Environment> _rootEnv;
};
} // namespace eval
} // namespace edn
} // namespace chainblocks

#endif
