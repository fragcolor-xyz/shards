#ifndef GENETIC_H
#define GENETIC_H

#include "shared.hpp"
#include "taskflow/core/executor.hpp"
#include "taskflow/core/taskflow.hpp"
#include <sstream>
#include <taskflow/taskflow.hpp>

namespace chainblocks {
namespace Genetic {
struct Evolve {
  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::FloatType; }
  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      _baseChain = value;
      break;
    case 1:
      _population = value.payload.intValue;
      break;
    case 2:
      _mutation = value.payload.floatValue;
      break;
    case 3:
      _extinction = value.payload.floatValue;
      break;
    case 4:
      _elitism = value.payload.floatValue;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return _baseChain;
    case 1:
      return Var(_population);
    case 2:
      return Var(_mutation);
    case 3:
      return Var(_extinction);
    case 4:
      return Var(_elitism);
    default:
      return CBVar();
    }
  }

  struct Writer {
    std::stringstream &_stream;
    Writer(std::stringstream &stream) : _stream(stream) {}
    void operator()(const uint8_t *buf, size_t size) {
      _stream.write((const char *)buf, size);
    }
  };

  struct Reader {
    std::stringstream &_stream;
    Reader(std::stringstream &stream) : _stream(stream) {}
    void operator()(uint8_t *buf, size_t size) {
      _stream.read((char *)buf, size);
    }
  };

  void cleanup() {
    if (_chains.size() > 0) {
      tf::Taskflow cleanupFlow;
      cleanupFlow.parallel_for(_chains.begin(), _chains.end(), [&](CBVar &var) {
        auto chain = CBChain::sharedFromRef(var.payload.chainValue);
        stop(chain.get());
        Serialization::varFree(var);
      });
      Tasks.run(cleanupFlow).get();
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    AsyncOp<InternalCore> op(context);
    return op([&]() {
      // Init on the first run!
      if (_chains.size() == 0) {
        std::stringstream chainStream;
        Writer w(chainStream);
        Serialization::serialize(_baseChain, w);
        auto chainStr = chainStream.str();
        _chains.resize(_population);
        tf::Taskflow initFlow;
        initFlow.parallel_for(_chains.begin(), _chains.end(), [&](CBVar &var) {
          std::stringstream inputStream(chainStr);
          Reader r(inputStream);
          Serialization::deserialize(r, var);
        });
        Tasks.run(initFlow).get();
      }
      // We run chains up to completion
      // From validation to end, every iteration
      tf::Taskflow runFlow;
      runFlow.parallel_for(_chains.begin(), _chains.end(), [&](CBVar &var) {
        CBNode node;
        auto chain = CBChain::sharedFromRef(var.payload.chainValue);
        node.schedule(chain.get());
        while (node.tick()) {
        }
      });
      Tasks.run(runFlow).get();
      // Find the best fitness.. do stuff...
      return Var(0.0);
    });
  }

private:
  tf::Executor &Tasks{Singleton<tf::Executor>::value};
  static inline Parameters _params{
      {"Fitness",
       "The fitness chain, should output a Float fitness value.",
       {CoreInfo::NoneType, CoreInfo::ChainType, CoreInfo::ChainVarType}},
      {"Population", "The population size.", {CoreInfo::IntType}},
      {"Mutation", "The rate of mutation, 0.1 = 10%.", {CoreInfo::FloatType}},
      {"Extinction",
       "The rate of extinction, 0.1 = 10%.",
       {CoreInfo::FloatType}},
      {"Elitism", "The rate of elitism, 0.1 = 10%.", {CoreInfo::FloatType}}};
  ParamVar _baseChain{};
  std::vector<CBVar> _chains;
  int64_t _population = 256;
  double _mutation = 0.2;
  double _extinction = 0.1;
  double _elitism = 0.1;
};

struct Mutant {};
} // namespace Genetic
} // namespace chainblocks

#endif /* GENETIC_H */
