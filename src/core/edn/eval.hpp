

#ifndef CB_LSP_EVAL_HPP
#define CB_LSP_EVAL_HPP

#include "read.hpp"

namespace chainblocks {
  namespace edn {
    namespace eval {
      class Environment;
      
      class Value {
      private:
	token::Token _token;
	std::weak_ptr<Environment> _owner;
      };

      class Environment {
      private:
	phmap::flat_hash_map<std::string, std::shared_ptr<Value>> _contents;
	std::weak_ptr<Environment> _parent;
      };

      class Program {
      public:
	Program() {
	  _rootEnv = std::make_shared<Environment>();
	}

	std::shared_ptr<Value> eval(std::list<form::Form> ast, shared_ptr<Environment> env) {
	  
	}

	std::shared_ptr<Value> eval(const std::string &code) {
	  auto tokens = read(code);
	  return eval(tokens, _rootEnv);
	}
	
      private:
	std::string _directory;
	std::shared_ptr<Environment> _rootEnv;
      };
    }
  }
}
