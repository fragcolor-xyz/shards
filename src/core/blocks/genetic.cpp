#ifndef GENETIC_H
#define GENETIC_H

#include "blockwrapper.hpp"
#include "chainblocks.h"
#include "chainblocks.hpp"
#include "shared.hpp"
#include <limits>
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
      _popsize = value.payload.intValue;
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
    case 5:
      _dnaNames = value;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return _baseChain;
    case 1:
      return Var(_popsize);
    case 2:
      return Var(_mutation);
    case 3:
      return Var(_extinction);
    case 4:
      return Var(_elitism);
    case 5:
      return _dnaNames;
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
    if (_population.size() > 0) {
      tf::Taskflow cleanupFlow;
      cleanupFlow.parallel_for(
          _population.begin(), _population.end(), [&](Individual &i) {
            // Free and release chain
            auto chain = CBChain::sharedFromRef(i.chain.payload.chainValue);
            stop(chain.get());
            Serialization::varFree(i.chain);
            // Cleanup variables too
            i.variables.clear();
          });
      Tasks.run(cleanupFlow).get();

      _chain2Individual.clear();
      _sortedPopulation.clear();
      _population.clear();
    }
    _dnaNames.cleanup();
    _baseChain.cleanup();
  }

  void warmup(CBContext *ctx) {
    _baseChain.warmup(ctx);
    _dnaNames.warmup(ctx);
  }

  inline void mutateChain(CBChain *chain);

  struct TickObserver {
    Evolve &self;

    void before_tick(CBChain *chain) {}
    void before_prepare(CBChain *chain) {}

    void before_stop(CBChain *chain) {
      // Collect DNA variables values
      auto names = self._dnaNames.get();
      if (names.valueType == Seq) {
        const auto individualIt = self._chain2Individual.find(chain);
        if (individualIt == self._chain2Individual.end()) {
          assert(false);
        }
        auto &individual = individualIt->second.get();
        IterableSeq snames(names.payload.seqValue);
        for (const auto &name : snames) {
          auto cname = name.payload.stringValue;
          individual.variables[cname] = chain->variables[cname];
        }
      }
    }

    void before_start(CBChain *chain) {
      // Do mutations
      const auto individualIt = self._chain2Individual.find(chain);
      if (individualIt == self._chain2Individual.end()) {
        assert(false);
      }
      auto &individual = individualIt->second.get();
      // Load variables if we have them
      for (auto &var : individual.variables) {
        auto &slot = chain->variables[var.first];
        cloneVar(slot, var.second);
      }
      // Call mutate
      self.mutateChain(chain);
    }
  };

  CBVar activate(CBContext *context, const CBVar &input) {
    AsyncOp<InternalCore> op(context);
    return op([&]() {
      // Init on the first run!
      // We reuse those chains for every era
      // Only the DNA changes
      if (_population.size() == 0) {
        std::stringstream chainStream;
        Writer w(chainStream);
        Serialization::serialize(_baseChain, w);
        auto chainStr = chainStream.str();
        _population.resize(_popsize);
        tf::Taskflow initFlow;
        initFlow.parallel_for(_population.begin(), _population.end(),
                              [&](Individual &i) {
                                std::stringstream inputStream(chainStr);
                                Reader r(inputStream);
                                Serialization::deserialize(r, i.chain);
                              });
        Tasks.run(initFlow).get();

        // Also populate chain2Indi
        for (auto &i : _population) {
          auto chain = CBChain::sharedFromRef(i.chain.payload.chainValue);
          _chain2Individual.emplace(chain.get(), i);
          _sortedPopulation.emplace_back(i);
        }
      }
      // We run chains up to completion
      // From validation to end, every iteration/era
      tf::Taskflow runFlow;
      runFlow.parallel_for(
          _population.begin(), _population.end(), [&](Individual &i) {
            CBNode node;
            TickObserver obs{*this};
            auto chain = CBChain::sharedFromRef(i.chain.payload.chainValue);
            node.schedule(obs, chain.get());
            while (!node.empty()) {
              node.tick(obs);
            }
          });
      Tasks.run(runFlow).get();

      std::sort(_sortedPopulation.begin(), _sortedPopulation.end(),
                [](std::reference_wrapper<Individual> a,
                   std::reference_wrapper<Individual> b) {
                  return a.get().fitness > b.get().fitness;
                });

      return Var(_sortedPopulation.back().get().fitness);
    });
  }

private:
  struct Individual {
    CBVar chain{};
    std::unordered_map<std::string, OwnedVar, std::hash<std::string>,
                       std::equal_to<std::string>,
                       boost::alignment::aligned_allocator<
                           std::pair<const std::string, OwnedVar>, 16>>
        variables;
    double fitness{std::numeric_limits<double>::min()};
  };

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
      {"Elitism", "The rate of elitism, 0.1 = 10%.", {CoreInfo::FloatType}},
      {"DNA",
       "A list of variable names that represent the DNA of the subject.",
       {CoreInfo::StringSeqType}}};
  ParamVar _baseChain{};
  ParamVar _dnaNames{};
  std::vector<Individual> _population;
  std::vector<std::reference_wrapper<Individual>> _sortedPopulation;
  std::unordered_map<CBChain *, std::reference_wrapper<Individual>>
      _chain2Individual;
  int64_t _popsize = 64;
  double _mutation = 0.2;
  double _extinction = 0.1;
  double _elitism = 0.1;
};

struct Mutant {
  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return CoreInfo::AnyType; }
  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, CBVar value) {
    switch (index) {
    case 0:
      _block = value;
      break;
    case 1:
      _options = value;
      break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return _block;
    case 1:
      return _options;
    default:
      return CBVar();
    }
  }

  void cleanup() {
    _block.cleanup();
    _options.cleanup();
  }

  void warmup(CBContext *ctx) {
    _block.warmup(ctx);
    _options.warmup(ctx);
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    return _block.compose(data).outputType;
  }

  CBExposedTypesInfo exposedVariables() {
    if (!_block)
      return {};

    auto blks = _block.blocks();
    return blks.elements[0]->exposedVariables(blks.elements[0]);
  }

  CBExposedTypesInfo requiredVariables() {
    if (!_block)
      return {};

    auto blks = _block.blocks();
    return blks.elements[0]->exposedVariables(blks.elements[0]);
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    return _block.activate(context, input);
  }

private:
  friend class Evolve;
  BlocksVar _block{};
  ParamVar _options{};
  static inline Parameters _params{
      {"Block", "The block to mutate.", {CoreInfo::BlockType}},
      {"Mutations",
       "Mutation table - a table with mutation options.",
       {CoreInfo::NoneType, CoreInfo::AnyTableType}}};
};

void registerBlocks() {
  REGISTER_CBLOCK("Evolve", Evolve);
  REGISTER_CBLOCK("Mutant", Mutant);
}

inline void Evolve::mutateChain(CBChain *chain) {
  std::vector<CBlockInfo> blocks;
  gatherBlocks(chain, blocks);
  auto pos =
      std::remove_if(std::begin(blocks), std::end(blocks),
                     [](CBlockInfo &info) { return info.name != "Mutant"; });
  if (pos != std::end(blocks)) {
    std::for_each(std::begin(blocks), pos, [](CBlockInfo &info) {
      auto mutator = reinterpret_cast<const BlockWrapper<Mutant> *>(info.block);
      if (mutator->block._block) {
        auto mutant = mutator->block._block.blocks().elements[0];
        if (mutant->mutate) {
          auto options = mutator->block._options;
          auto table = options.get().valueType == Table
                           ? options.get().payload.tableValue
                           : CBTable();
          mutant->mutate(mutant, table);
        }
      }
    });
  }
}
} // namespace Genetic
} // namespace chainblocks

#endif /* GENETIC_H */
