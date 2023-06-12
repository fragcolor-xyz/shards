/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2019 Fragcolor Pte. Ltd. */

#ifndef SH_LSP_EVAL_HPP
#define SH_LSP_EVAL_HPP

/*
 * This is an experiment
 * needs major refactor of how variant is used
 * should use std::visit
 * also read is not our code yet could use same refactoring
 */

#include "../runtime.hpp"
#include "print.hpp"
#include "read.hpp"

namespace shards {
namespace edn {
namespace eval {
class EvalException : public std::exception {
public:
  explicit EvalException(std::string errmsg, int line) : errorMessage(errmsg) {
    errorMessage = "\nScript evaluation error!\nline: " + std::to_string(line) + "\nerror: " + errorMessage;
  }

  [[nodiscard]] const char *what() const noexcept override { return errorMessage.c_str(); }

private:
  std::string errorMessage;
};

namespace value {
enum types { Var, Lambda, Mesh, Form, BuiltIn };
}

class Environment;
class SHVarValue;
class Lambda;
class Mesh;
class BuiltIn;
using Value = std::variant<SHVarValue, Lambda, Mesh, form::Form, std::shared_ptr<BuiltIn>>;

class ValueBase {
public:
  ValueBase(const token::Token &token, const std::shared_ptr<Environment> &env) : _token(token), _owner(env) {}

  virtual std::string pr_str(document &doc) { return ::shards::edn::pr_str(doc, _token); }

protected:
  token::Token _token;
  std::weak_ptr<Environment> _owner;
};

class SHVarValue : public ValueBase {
public:
  SHVarValue(const token::Token &token, const std::shared_ptr<Environment> &env) : ValueBase(token, env), _var({}) {}

  const SHVar &value() const { return _var; }

protected:
  SHVar _var;
};

class NilValue : public SHVarValue {
public:
  NilValue(const token::Token &token, const std::shared_ptr<Environment> &env) : SHVarValue(token, env) {}
};

class IntValue : public SHVarValue {
public:
  IntValue(int64_t value, const token::Token &token, const std::shared_ptr<Environment> &env) : SHVarValue(token, env) {
    _var.valueType = SHType::Int;
    _var.payload.intValue = value;
  }
};

class FloatValue : public SHVarValue {
public:
  FloatValue(double value, const token::Token &token, const std::shared_ptr<Environment> &env) : SHVarValue(token, env) {
    _var.valueType = SHType::Float;
    _var.payload.floatValue = value;
  }
};

class StringValue : public SHVarValue {
public:
  StringValue(std::string value, const token::Token &token, const std::shared_ptr<Environment> &env)
      : SHVarValue(token, env), _storage(value) {
    _var.valueType = SHType::String;
    _var.payload.stringValue = _storage.c_str();
  }

private:
  std::string _storage;
};

class BoolValue : public SHVarValue {
public:
  BoolValue(bool value, const token::Token &token, const std::shared_ptr<Environment> &env) : SHVarValue(token, env) {
    _var.valueType = SHType::Bool;
    _var.payload.boolValue = value;
  }
};

class ShardValue : public SHVarValue {
public:
  ShardValue(ShardPtr value, const token::Token &token, const std::shared_ptr<Environment> &env) : SHVarValue(token, env) {
    _var.valueType = SHType::ShardRef;
    _var.payload.shardValue = value;
    _shard = std::shared_ptr<Shard>(value, [this](Shard *blk) {
      // Make sure we are not consumed (inside a wire or another shard)
      if (_var.valueType == SHType::ShardRef)
        blk->destroy(blk);
    });
  }

  void consume() {
    if (_var.valueType != SHType::ShardRef) {
      throw EvalException("Attempt to use an already consumed shard", _token.line);
    }
    _var = {};
  }

private:
  std::shared_ptr<Shard> _shard;
};

class Lambda : public ValueBase {
public:
  Lambda(const std::vector<std::string> &argNames, const form::FormWrapper &body, const token::Token &token,
         const std::shared_ptr<Environment> &env)
      : ValueBase(token, env), _argNames(argNames), _body(body) {}

  const form::Form &body() const { return _body.form; }
  const std::vector<std::string> &args() { return _argNames; }

private:
  std::vector<std::string> _argNames;
  form::FormWrapper _body;
};

class Mesh : public ValueBase {
public:
  Mesh(const token::Token &token, const std::shared_ptr<Environment> &env) : ValueBase(token, env) { _mesh = SHMesh::make(); }

private:
  std::shared_ptr<SHMesh> _mesh;
};

class Environment;
class Program {
public:
  Program() { _rootEnv = std::make_shared<Environment>(); }

  static Value eval(form::Form ast, std::shared_ptr<Environment> env, int *line);

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

class BuiltIn {
public:
  virtual Value apply(const std::shared_ptr<Environment> &env, std::vector<Value> &args, int line) {
    SHLOG_FATAL("invalid state");
    return args[0]; // fake value should not happen
  }
};

class First : public BuiltIn {
public:
  virtual ~First() {}
  Value apply(const std::shared_ptr<Environment> &env, std::vector<Value> &args, int line) override {
    if (args.size() != 1) {
      throw EvalException("first, expected a single argument", line);
    }
    auto &v = args[0];
    if (v.index() == value::types::Form) {
      auto &f = std::get<form::Form>(v);
      if (f.index() == form::LIST) {
        auto &list = std::get<std::list<form::FormWrapper>>(f);
        return Program::eval(list.front().form, env, &line);
      } else if (f.index() == form::VECTOR) {
        auto &vec = std::get<std::vector<form::FormWrapper>>(f);
        return Program::eval(vec[0].form, env, &line);
      } else {
        throw EvalException("first, list or vector expected", line);
      }
    } else {
      throw EvalException("first, list or vector expected", line);
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
} // namespace eval
} // namespace edn
} // namespace shards

#endif