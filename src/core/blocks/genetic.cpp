#include "blockwrapper.hpp"
#include "chainblocks.h"
#include "chainblocks.hpp"
#include "shared.hpp"
#include "taskflow/core/executor.hpp"
#include <limits>
#include <pdqsort.h>
#include <random>
#include <sstream>
#include <taskflow/taskflow.hpp>

namespace chainblocks {
namespace Genetic {

struct Random {
  static double nextDouble() { return _udis(_gen); }
  static int nextInt() { return _uintdis(_gen); }
  static double nextNormal() { return _ndis(_gen); }
  static double nextNormal1() { return _ndis1(_gen); }

  static inline thread_local std::random_device _rd{};
  static inline thread_local std::mt19937 _gen{_rd()};
  static inline thread_local std::uniform_int_distribution<> _uintdis{};
  static inline thread_local std::uniform_real_distribution<> _udis{0.0, 1.0};
  static inline thread_local std::normal_distribution<> _ndis{0.0, 0.1};
  static inline thread_local std::normal_distribution<> _ndis1{0.0, 1.0};
};

struct Mutant;
struct Evolve {
  static CBTypesInfo inputTypes() { return CoreInfo::AnyType; }
  static CBTypesInfo outputTypes() { return _outputType; }
  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _baseChain = value;
      break;
    case 1:
      _fitnessChain = value;
      break;
    case 2:
      _popsize = value.payload.intValue;
      break;
    case 3:
      _mutation = value.payload.floatValue;
      break;
    case 4:
      _crossover = value.payload.floatValue;
      break;
    case 5:
      _extinction = value.payload.floatValue;
      break;
    case 6:
      _elitism = value.payload.floatValue;
      break;
    case 7:
      _threads = std::max(int64_t(1), value.payload.intValue);
      break;
    case 8:
      _coros = value.payload.intValue;
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
      return _fitnessChain;
    case 2:
      return Var(_popsize);
    case 3:
      return Var(_mutation);
    case 4:
      return Var(_crossover);
    case 5:
      return Var(_extinction);
    case 6:
      return Var(_elitism);
    case 7:
      return Var(_threads);
    case 8:
      return Var(_coros);
    default:
      return Var::Empty;
    }
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    // we still want to run a compose on the subject, in order
    // to serialize it properly!
    assert(_baseChain.valueType == CBType::Chain);
    assert(_fitnessChain.valueType == CBType::Chain);

    auto bchain = CBChain::sharedFromRef(_baseChain.payload.chainValue);
    auto fchain = CBChain::sharedFromRef(_fitnessChain.payload.chainValue);

    CBInstanceData vdata{};
    vdata.chain = data.chain;
    auto res = composeChain(
        bchain.get(),
        [](const CBlock *errorBlock, const char *errorTxt, bool nonfatalWarning,
           void *userData) {
          if (!nonfatalWarning) {
            LOG(ERROR) << "Evolve: failed subject chain validation, error: "
                       << errorTxt;
            throw CBException("Evolve: failed subject chain validation");
          } else {
            LOG(INFO) << "Evolve: warning during subject chain validation: "
                      << errorTxt;
          }
        },
        this, vdata);
    arrayFree(res.exposedInfo);
    arrayFree(res.requiredInfo);

    vdata.inputType = res.outputType;
    res = composeChain(
        fchain.get(),
        [](const CBlock *errorBlock, const char *errorTxt, bool nonfatalWarning,
           void *userData) {
          if (!nonfatalWarning) {
            LOG(ERROR) << "Evolve: failed fitness chain validation, error: "
                       << errorTxt;
            throw CBException("Evolve: failed fitness chain validation");
          } else {
            LOG(INFO) << "Evolve: warning during fitness chain validation: "
                      << errorTxt;
          }
        },
        this, vdata);
    if (res.outputType.basicType != CBType::Float) {
      throw ComposeError(
          "Evolve: fitness chain should output a Float, but instead got " +
          type2Name(res.outputType.basicType));
    }
    arrayFree(res.exposedInfo);
    arrayFree(res.requiredInfo);

    return _outputType;
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

  void warmup(CBContext *context) {
    if (!_exec || _exec->num_workers() != (size_t(_threads) + 1)) {
      _exec.reset(new tf::Executor(size_t(_threads) + 1));
    }
  }

  void cleanup() {
    if (_population.size() > 0) {
      tf::Taskflow cleanupFlow;
      cleanupFlow.for_each_dynamic(
          _population.begin(), _population.end(), [&](Individual &i) {
            // Free and release chain
            auto chain = CBChain::sharedFromRef(i.chain.payload.chainValue);
            stop(chain.get());
            Serialization::varFree(i.chain);
            auto fitchain =
                CBChain::sharedFromRef(i.fitnessChain.payload.chainValue);
            stop(fitchain.get());
            Serialization::varFree(i.fitnessChain);
            i.node->terminate();
          });
      _exec->run(cleanupFlow).get();

      _sortedPopulation.clear();
      _population.clear();
      _exec.reset(nullptr);
    }
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    return awaitne(context, [&]() {
      // Init on the first run!
      // We reuse those chains for every era
      // Only the DNA changes
      if (_population.size() == 0) {
        LOG(TRACE) << "Evolve, first run, init";

        Serialization serial;
        std::stringstream chainStream;
        Writer w1(chainStream);
        serial.reset();
        serial.serialize(_baseChain, w1);
        auto chainStr = chainStream.str();

        std::stringstream fitnessStream;
        Writer w2(fitnessStream);
        serial.reset();
        serial.serialize(_fitnessChain, w2);
        auto fitnessStr = fitnessStream.str();

        _population.resize(_popsize);
        _nkills = size_t(double(_popsize) * _extinction);
        _nelites = size_t(double(_popsize) * _elitism);

        tf::Taskflow initFlow;
        initFlow.for_each_dynamic(
            _population.begin(), _population.end(), [&](Individual &i) {
              Serialization deserial;
              std::stringstream i1Stream(chainStr);
              Reader r1(i1Stream);
              deserial.reset();
              deserial.deserialize(r1, i.chain);
              auto chain = CBChain::sharedFromRef(i.chain.payload.chainValue);
              gatherMutants(chain.get(), i.mutants);
              resetState(i);

              std::stringstream i2Stream(fitnessStr);
              Reader r2(i2Stream);
              deserial.reset();
              deserial.deserialize(r2, i.fitnessChain);
            });
        _exec->run(initFlow).get();

        size_t idx = 0;
        for (auto &i : _population) {
          i.idx = idx;
          _sortedPopulation.emplace_back(&i);
          idx++;
        }

        _era = 0;
      } else {
        LOG(TRACE) << "Evolve, crossover";

        // do crossover here, populating tasks properly
        tf::Taskflow crossoverFlow;
        _crossingOver.clear();
        int currentIdx = 0;
        for (auto &ind : _sortedPopulation) {
          ind->crossoverTask.reset();
          if (Random::nextDouble() < _crossover) {
            // In this case this individual
            // becomes the child between two other individuals
            // there is a chance also to keep current values
            // so this is effectively tree way crossover
            // Select from high fitness individuals
            const auto parent0Idx =
                int(std::pow(Random::nextDouble(), 4) * double(_popsize));
            auto parent0 = _sortedPopulation[parent0Idx];

            const auto parent1Idx =
                int(std::pow(Random::nextDouble(), 4) * double(_popsize));
            auto parent1 = _sortedPopulation[parent1Idx];

            if (currentIdx != parent0Idx && currentIdx != parent1Idx &&
                parent0Idx != parent1Idx && parent0->parent0Idx != currentIdx &&
                parent0->parent1Idx != currentIdx &&
                parent1->parent0Idx != currentIdx &&
                parent1->parent1Idx != currentIdx) {
              ind->crossoverTask =
                  crossoverFlow.emplace([this, ind, parent0, parent1]() {
                    crossover(*ind, *parent0, *parent1);
                  });
#if 0
              ind.get().crossoverTask.name(std::to_string(currentIdx) + " = " +
                                           std::to_string(parent0Idx) + " + " +
                                           std::to_string(parent1Idx));
#endif
              _crossingOver.emplace_back(ind, parent0, parent1);
              ind->parent0Idx = parent0Idx;
              ind->parent1Idx = parent1Idx;
            }
          }
          currentIdx++;
        }

        for (auto [a, b, c] : _crossingOver) {
          auto &atask = a->crossoverTask;
          auto &btask = b->crossoverTask;
          auto &ctask = c->crossoverTask;
          if (!btask.empty())
            btask.precede(atask);
          if (!ctask.empty())
            ctask.precede(atask);
        }
#if 0
        crossoverFlow.dump(std::cout);
#endif
        _exec->run(crossoverFlow).get();

        _era++;

        // hack/fix
        // it is likely possible that the best chain we outputted
        // was used and so warmedup
        auto best = CBChain::sharedFromRef(
            _sortedPopulation.front()->chain.payload.chainValue);
        best->cleanup(true);
      }

#if 1
      // We run chains up to completion
      // From validation to end, every iteration/era
      // We run in such a way to allow coroutines + threads properly
      LOG(TRACE) << "Evolve, schedule chains";
      {
        tf::Taskflow flow;

        flow.for_each_dynamic(
            _era == 0 ? _sortedPopulation.begin()
                      : _sortedPopulation.begin() + _nelites,
            _sortedPopulation.end(),
            [](auto &i) {
              // Evaluate our brain chain
              auto chain = CBChain::sharedFromRef(i->chain.payload.chainValue);
              i->node->schedule(chain);
            },
            _coros);

        _exec->run(flow).get();
      }
      LOG(TRACE) << "Evolve, run chains";
      {
        tf::Taskflow flow;

        flow.for_each_dynamic(
            _era == 0 ? _sortedPopulation.begin()
                      : _sortedPopulation.begin() + _nelites,
            _sortedPopulation.end(),
            [](auto &i) {
              if (!i->node->empty())
                i->node->tick();
            },
            _coros);

        _exec
            ->run_until(flow,
                        [&]() {
                          size_t ended = 0;
                          for (auto &p : _population) {
                            if (p.node->empty())
                              ended++;
                          }
                          return ended == _population.size();
                        })
            .get();
      }
      LOG(TRACE) << "Evolve, schedule fitness";
      {
        tf::Taskflow flow;

        flow.for_each_dynamic(
            _era == 0 ? _sortedPopulation.begin()
                      : _sortedPopulation.begin() + _nelites,
            _sortedPopulation.end(),
            [](auto &i) {
              // compute the fitness
              TickObserver obs{*i};
              auto fitchain =
                  CBChain::sharedFromRef(i->fitnessChain.payload.chainValue);
              auto chain = CBChain::sharedFromRef(i->chain.payload.chainValue);
              i->node->schedule(obs, fitchain, chain->finishedOutput);
            },
            _coros);

        _exec->run(flow).get();
      }
      LOG(TRACE) << "Evolve, run fitness";
      {
        tf::Taskflow flow;

        flow.for_each_dynamic(
            _era == 0 ? _sortedPopulation.begin()
                      : _sortedPopulation.begin() + _nelites,
            _sortedPopulation.end(),
            [](auto &i) {
              if (!i->node->empty()) {
                TickObserver obs{*i};
                i->node->tick(obs);
              }
            },
            _coros);

        _exec
            ->run_until(flow,
                        [&]() {
                          size_t ended = 0;
                          for (auto &p : _population) {
                            if (p.node->empty())
                              ended++;
                          }
                          return ended == _population.size();
                        })
            .get();
      }
#else
      // The following is for reference and for full TSAN runs

      // We run chains up to completion
      // From validation to end, every iteration/era
      {
        tf::Taskflow runFlow;
        runFlow.for_each_dynamic(
            _population.begin(), _population.end(), [&](Individual &i) {
              TickObserver obs{i};

              // Evaluate our brain chain
              auto chain = CBChain::sharedFromRef(i.chain.payload.chainValue);
              i.node->schedule(chain);
              while (!node.empty()) {
                i.node->tick();
              }

              // compute the fitness
              auto fitchain =
                  CBChain::sharedFromRef(i.fitnessChain.payload.chainValue);
              i.node->schedule(obs, fitchain, chain->finishedOutput);
              while (!i.node->empty()) {
                i.node->tick(obs);
              }
            });
        _exec->run(runFlow).get();
      }
#endif
      LOG(TRACE) << "Evolve, stopping all chains";
      { // Stop all the population chains
        tf::Taskflow flow;

        flow.for_each_dynamic(
            _population.begin(), _population.end(), [](Individual &i) {
              auto chain = CBChain::sharedFromRef(i.chain.payload.chainValue);
              auto fitchain =
                  CBChain::sharedFromRef(i.fitnessChain.payload.chainValue);
              stop(chain.get());
              chain->composedHash = 0;
              stop(fitchain.get());
              fitchain->composedHash = 0;
              i.node->terminate();
            });

        _exec->run(flow).get();
      }

      LOG(TRACE) << "Evolve, sorting";
      // remove non normal fitness (sort needs this or crashes will happen)
      std::for_each(_sortedPopulation.begin(), _sortedPopulation.end(),
                    [](auto &i) {
                      if (!std::isnormal(i->fitness)) {
                        i->fitness = -std::numeric_limits<float>::max();
                      }
                    });
      pdqsort(
          _sortedPopulation.begin(), _sortedPopulation.end(),
          [](const auto &a, const auto &b) { return a->fitness > b->fitness; });

      LOG(TRACE) << "Evolve, resetting flags";
      // reset flags
      std::for_each(_sortedPopulation.begin(),
                    _sortedPopulation.end() - _nkills, [](auto &i) {
                      i->extinct = false;
                      i->parent0Idx = -1;
                      i->parent1Idx = -1;
                    });
      std::for_each(_sortedPopulation.end() - _nkills, _sortedPopulation.end(),
                    [](auto &i) {
                      i->extinct = true;
                      i->parent0Idx = -1;
                      i->parent1Idx = -1;
                    });

      LOG(TRACE) << "Evolve, run mutations";
      // Do mutations at end, yet when contexts are still valid!
      // since we might need them
      {
        tf::Taskflow mutFlow;
        mutFlow.for_each_dynamic(_sortedPopulation.begin() + _nelites,
                                 _sortedPopulation.end(), [&](auto &i) {
                                   // reset the individual if extinct
                                   if (i->extinct) {
                                     resetState(*i);
                                   }
                                   mutate(*i);
                                 });
        _exec->run(mutFlow).get();
      }

      LOG(TRACE) << "Evolve, era done";
      _result.clear();
      _result.emplace_back(Var(_sortedPopulation.front()->fitness));
      _result.emplace_back(_sortedPopulation.front()->chain);
      return Var(_result);
    });
  }

private:
  struct MutantInfo {
    MutantInfo(const Mutant &block) : block(block) {}

    std::reference_wrapper<const Mutant> block;

    // For reset... hmm we copy them due to multi thread
    // ideally they are all the same tho..
    std::vector<std::tuple<int, OwnedVar>> originalParams;
  };

  static inline void gatherMutants(CBChain *chain,
                                   std::vector<MutantInfo> &out);

  struct Individual {
    ~Individual() {
      Serialization::varFree(chain);
      Serialization::varFree(fitnessChain);
    }

    size_t idx = 0;

    // Chains are recycled
    CBVar chain{};
    // We need many of them cos we use threads
    CBVar fitnessChain{};
    // The node we run on
    std::shared_ptr<CBNode> node{CBNode::make()};

    // Keep track of mutants and push/pop mutations on chain
    std::vector<MutantInfo> mutants;

    double fitness{-std::numeric_limits<float>::max()};

    bool extinct = false;

    tf::Task crossoverTask;
    int parent0Idx = -1;
    int parent1Idx = -1;
  };

  struct TickObserver {
    Individual &self;

    void before_tick(CBChain *chain) {}
    void before_start(CBChain *chain) {}
    void before_prepare(CBChain *chain) {}

    void before_stop(CBChain *chain) {
      // Collect fitness last result
      auto fitnessVar = chain->finishedOutput;
      if (fitnessVar.valueType == Float) {
        self.fitness = fitnessVar.payload.floatValue;
      }
    }

    void before_compose(CBChain *chain) {
      // Keep in mind that individuals might not reach chain termination
      // they might "crash", so fitness should be set to minimum before any run
      self.fitness = -std::numeric_limits<float>::max();
    }
  };

  inline void crossover(Individual &child, const Individual &parent0,
                        const Individual &parent1);
  inline void mutate(Individual &individual);
  inline void resetState(Individual &individual);

  static inline Parameters _params{
      {"Chain",
       CBCCSTR("The chain to optimize and evolve."),
       {CoreInfo::ChainType}},
      {"Fitness",
       CBCCSTR(
           "The fitness chain to run at the end of the main chain evaluation "
           "and using its last output; should output a Float fitness value."),
       {CoreInfo::ChainType}},
      {"Population", CBCCSTR("The population size."), {CoreInfo::IntType}},
      {"Mutation",
       CBCCSTR("The rate of mutation, 0.1 = 10%."),
       {CoreInfo::FloatType}},
      {"Crossover",
       CBCCSTR("The rate of crossover, 0.1 = 10%."),
       {CoreInfo::FloatType}},
      {"Extinction",
       CBCCSTR("The rate of extinction, 0.1 = 10%."),
       {CoreInfo::FloatType}},
      {"Elitism",
       CBCCSTR("The rate of elitism, 0.1 = 10%."),
       {CoreInfo::FloatType}},
      {"Threads",
       CBCCSTR("The number of cpu threads to use."),
       {CoreInfo::IntType}},
      {"Coroutines",
       CBCCSTR("The number of coroutines to run on each thread."),
       {CoreInfo::IntType}}};
  static inline Types _outputTypes{{CoreInfo::FloatType, CoreInfo::ChainType}};
  static inline Type _outputType{{CBType::Seq, {.seqTypes = _outputTypes}}};

  std::unique_ptr<tf::Executor> _exec;

  OwnedVar _baseChain{};
  OwnedVar _fitnessChain{};
  std::vector<CBVar> _result;
  std::vector<Individual> _population;
  std::vector<Individual *> _sortedPopulation;
  std::vector<std::tuple<Individual *, Individual *, Individual *>>
      _crossingOver;
  int64_t _popsize = 64;
  int64_t _coros = 8;
  int64_t _threads = 2;
  double _mutation = 0.2;
  double _crossover = 0.2;
  double _extinction = 0.1;
  size_t _nkills = 0;
  double _elitism = 0.1;
  size_t _nelites = 0;
  size_t _era = 0;
};

struct Mutant {
  CBTypesInfo inputTypes() {
    if (_block) {
      auto blks = _block.blocks();
      return blks.elements[0]->inputTypes(blks.elements[0]);
    } else {
      return CoreInfo::AnyType;
    }
  }

  CBTypesInfo outputTypes() {
    if (_block) {
      auto blks = _block.blocks();
      return blks.elements[0]->outputTypes(blks.elements[0]);
    } else {
      return CoreInfo::AnyType;
    }
  }

  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0:
      _block = value;
      break;
    case 1:
      _indices = value;
      break;
    case 2: {
      destroyBlocks();
      _mutations = value;
      if (_mutations.valueType == Seq) {
        for (auto &mut : _mutations) {
          if (mut.valueType == Block) {
            auto blk = mut.payload.blockValue;
            blk->owned = true;
          } else if (mut.valueType == Seq) {
            for (auto &bv : mut) {
              auto blk = bv.payload.blockValue;
              blk->owned = true;
            }
          }
        }
      }
    } break;
    case 3:
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
      return _indices;
    case 2:
      return _mutations;
    case 3:
      return _options;
    default:
      return CBVar();
    }
  }

  void cleanupMutations() const {
    if (_mutations.valueType == Seq) {
      for (auto &mut : _mutations) {
        if (mut.valueType == Block) {
          auto blk = mut.payload.blockValue;
          blk->cleanup(blk);
        } else if (mut.valueType == Seq) {
          for (auto &bv : mut) {
            auto blk = bv.payload.blockValue;
            blk->cleanup(blk);
          }
        }
      }
    }
  }

  void warmupMutations(CBContext *ctx) const {
    if (_mutations.valueType == Seq) {
      for (auto &mut : _mutations) {
        if (mut.valueType == Block) {
          auto blk = mut.payload.blockValue;
          if (blk->warmup)
            blk->warmup(blk, ctx);
        } else if (mut.valueType == Seq) {
          for (auto &bv : mut) {
            auto blk = bv.payload.blockValue;
            if (blk->warmup)
              blk->warmup(blk, ctx);
          }
        }
      }
    }
  }

  void cleanup() {
    _block.cleanup();
    cleanupMutations();
  }

  void destroyBlocks() {
    if (_mutations.valueType == Seq) {
      for (auto &mut : _mutations) {
        if (mut.valueType == Block) {
          auto blk = mut.payload.blockValue;
          blk->cleanup(blk);
          blk->destroy(blk);
        } else if (mut.valueType == Seq) {
          for (auto &bv : mut) {
            auto blk = bv.payload.blockValue;
            blk->cleanup(blk);
            blk->destroy(blk);
          }
        }
      }
    }
  }

  void destroy() { destroyBlocks(); }

  void warmup(CBContext *ctx) { _block.warmup(ctx); }

  CBTypeInfo compose(const CBInstanceData &data) {
    auto inner = mutant();
    // validate parameters
    if (_mutations.valueType == Seq && inner) {
      auto dataCopy = data;
      int idx = 0;
      auto innerParams = inner->parameters(inner);
      for (auto &mut : _mutations) {
        if (idx >= int(innerParams.len))
          break;
        TypeInfo ptype(inner->getParam(inner, idx));
        dataCopy.inputType = ptype;
        if (mut.valueType == Block) {
          auto blk = mut.payload.blockValue;
          if (blk->compose) {
            auto res = blk->compose(blk, dataCopy);
            if (res != ptype) {
              throw CBException("Expected same type as input in parameter "
                                "mutation chain's output.");
            }
          }
        } else if (mut.valueType == Seq) {
          auto res = composeChain(
              mut.payload.seqValue,
              [](const CBlock *errorBlock, const char *errorTxt,
                 bool nonfatalWarning, void *userData) {},
              nullptr, dataCopy);
          if (res.outputType != ptype) {
            throw CBException("Expected same type as input in parameter "
                              "mutation chain's output.");
          }
          arrayFree(res.exposedInfo);
          arrayFree(res.requiredInfo);
        }
        idx++;
      }
    }

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
    CBVar output{};
    _block.activate(context, input, output);
    return output;
  }

  CBlock *mutant() const {
    if (!_block)
      return nullptr;

    auto blks = _block.blocks();
    return blks.elements[0];
  }

private:
  friend struct Evolve;
  BlocksVar _block{};
  OwnedVar _indices{};
  OwnedVar _mutations{};
  OwnedVar _options{};
  static inline Parameters _params{
      {"Block", CBCCSTR("The block to mutate."), {CoreInfo::BlockType}},
      {"Indices",
       CBCCSTR("The parameter indices to mutate of the inner block."),
       {CoreInfo::IntSeqType}},
      {"Mutations",
       CBCCSTR("Optional chains of blocks (or Nones) to call when mutating one "
               "of the parameters, if empty a default operator will be used."),
       {CoreInfo::BlocksOrNoneSeq}},
      {"Options",
       CBCCSTR("Mutation options table - a table with mutation options."),
       {CoreInfo::NoneType, CoreInfo::AnyTableType}}};
};

inline void mutateVar(CBVar &var) {
  switch (var.valueType) {
  case CBType::Int: {
    const auto add = Random::nextNormal1() * double(var.payload.intValue);
    var.payload.intValue += int64_t(add);
  } break;
  case CBType::Int2: {
    for (auto i = 0; i < 2; i++) {
      const auto add = Random::nextNormal1() * double(var.payload.int2Value[i]);
      var.payload.int2Value[i] += int64_t(add);
    }
  } break;
  case CBType::Int3: {
    for (auto i = 0; i < 3; i++) {
      const auto add = Random::nextNormal1() * double(var.payload.int3Value[i]);
      var.payload.int3Value[i] += int32_t(add);
    }
  } break;
  case CBType::Int4: {
    for (auto i = 0; i < 4; i++) {
      const auto add = Random::nextNormal1() * double(var.payload.int4Value[i]);
      var.payload.int4Value[i] += int32_t(add);
    }
  } break;
  case CBType::Int8: {
    for (auto i = 0; i < 8; i++) {
      const auto add = Random::nextNormal1() * double(var.payload.int8Value[i]);
      var.payload.int8Value[i] += int16_t(add);
    }
  } break;
  case CBType::Int16: {
    for (auto i = 0; i < 16; i++) {
      const auto add =
          Random::nextNormal1() * double(var.payload.int16Value[i]);
      var.payload.int16Value[i] += int8_t(add);
    }
  } break;
  case CBType::Float: {
    var.payload.floatValue += Random::nextNormal();
  } break;
  case CBType::Float2: {
    for (auto i = 0; i < 2; i++)
      var.payload.float2Value[i] += Random::nextNormal();
  } break;
  case CBType::Float3: {
    for (auto i = 0; i < 3; i++)
      var.payload.float3Value[i] += float(Random::nextNormal());
  } break;
  case CBType::Float4: {
    for (auto i = 0; i < 4; i++)
      var.payload.float4Value[i] += float(Random::nextNormal());
  } break;
  default:
    break;
  }
}

inline void Evolve::gatherMutants(CBChain *chain,
                                  std::vector<MutantInfo> &out) {
  std::vector<CBlockInfo> blocks;
  gatherBlocks(chain, blocks);
  auto pos =
      std::remove_if(std::begin(blocks), std::end(blocks),
                     [](CBlockInfo &info) { return info.name != "Mutant"; });
  if (pos != std::end(blocks)) {
    std::for_each(std::begin(blocks), pos, [&](CBlockInfo &info) {
      auto mutator = reinterpret_cast<const BlockWrapper<Mutant> *>(info.block);
      auto &minfo = out.emplace_back(mutator->block);
      auto mutant = mutator->block.mutant();
      if (mutator->block._indices.valueType == Seq) {
        for (auto &idx : mutator->block._indices) {
          auto i = int(idx.payload.intValue);
          minfo.originalParams.emplace_back(i, mutant->getParam(mutant, i));
        }
      }
    });
  }
}

inline void Evolve::crossover(Individual &child, const Individual &parent0,
                              const Individual &parent1) {
  auto cmuts = child.mutants.cbegin();
  auto p0muts = parent0.mutants.cbegin();
  auto p1muts = parent1.mutants.cbegin();
  for (; cmuts != child.mutants.end() && p0muts != parent0.mutants.end() &&
         p1muts != parent1.mutants.end();
       ++cmuts, ++p0muts, ++p1muts) {
    auto cb = cmuts->block.get().mutant();
    auto p0b = p0muts->block.get().mutant();
    auto p1b = p1muts->block.get().mutant();
    if (cb && p0b && p1b) {
      // use crossover proc if avail
      if (cb->crossover) {
        auto p0s = p0b->getState(p0b);
        auto p1s = p1b->getState(p1b);
        cb->crossover(cb, &p0s, &p1s);
      }
      // check if we have mutant params and cross them over
      auto &indices = cmuts->block.get()._indices;
      if (indices.valueType == Seq) {
        for (auto &idx : indices) {
          const auto i = int(idx.payload.intValue);
          const auto r = Random::nextDouble();
          if (r < 0.33) {
            // take from 0
            auto val = p0b->getParam(p0b, i);
            cb->setParam(cb, i, &val);
          } else if (r < 0.66) {
            // take from 1
            auto val = p1b->getParam(p1b, i);
            cb->setParam(cb, i, &val);
          } // else we keep
        }
      }
    }
  }
}

inline void Evolve::mutate(Evolve::Individual &individual) {
  auto chain = CBChain::sharedFromRef(individual.chain.payload.chainValue);
  // we need to hack this in as we run out of context
  CBCoro foo{};
  CBFlow flow{};
#ifndef __EMSCRIPTEN__
  CBContext ctx(std::move(foo), chain.get(), &flow);
#else
  CBContext ctx(&foo, chain.get(), &flow);
#endif
  ctx.chainStack.push_back(chain.get());
  std::for_each(
      std::begin(individual.mutants), std::end(individual.mutants),
      [&](MutantInfo &info) {
        if (Random::nextDouble() > _mutation) {
          return;
        }

        auto &mutator = info.block.get();
        auto &options = mutator._options;
        if (info.block.get().mutant()) {
          auto mutant = info.block.get().mutant();
          auto &indices = mutator._indices;
          if (mutant->mutate && (indices.valueType == None || rand() < 0.5)) {
            // In the case the block has `mutate`
            auto table = options.valueType == Table ? options.payload.tableValue
                                                    : CBTable();
            mutant->mutate(mutant, table);
          } else if (indices.valueType == Seq) {
            auto &iseq = indices.payload.seqValue;
            // do stuff on the param
            // select a random one
            auto rparam = Random::nextInt() % iseq.len;
            auto current = mutant->getParam(
                mutant, int(iseq.elements[rparam].payload.intValue));
            // if we have mutation blocks use them
            // if not use default operation
            if (mutator._mutations.valueType == Seq &&
                uint32_t(rparam) < mutator._mutations.payload.seqValue.len) {
              // we need to warmup / cleanup in this case
              // mutant mini chain also currently is not composed! FIXME?
              mutator.warmupMutations(&ctx);
              auto mblks = mutator._mutations.payload.seqValue.elements[rparam];
              if (mblks.valueType == Block) {
                auto blk = mblks.payload.blockValue;
                current = blk->activate(blk, &ctx, &current);
              } else if (mblks.valueType == Seq) {
                auto blks = mblks.payload.seqValue;
                CBVar out{};
                activateBlocks(blks, &ctx, current, out);
                current = out;
              } else {
                // Was likely None, so use default op
                mutateVar(current);
              }
              mutator.cleanupMutations();
            } else {
              mutateVar(current);
            }
            mutant->setParam(
                mutant, int(iseq.elements[rparam].payload.intValue), &current);
          }
        }
      });
}

inline void Evolve::resetState(Evolve::Individual &individual) {
  // Reset params and state
  std::for_each(std::begin(individual.mutants), std::end(individual.mutants),
                [](MutantInfo &info) {
                  auto mutant = info.block.get().mutant();
                  if (!mutant)
                    return;

                  if (mutant->resetState)
                    mutant->resetState(mutant);

                  for (auto [idx, val] : info.originalParams) {
                    mutant->setParam(mutant, idx, &val);
                  }
                });
}

struct DBlock {
  static CBOptionalString help() { return CBCCSTR("A dynamic block."); }

  static inline Parameters _params{
      {"Name",
       CBCCSTR("The name of the block to wrap."),
       {CoreInfo::StringType}},
      {"Parameters",
       CBCCSTR("The parameters to pass to the wrapped block."),
       {CoreInfo::AnySeqType}}};

  static CBParametersInfo parameters() { return _params; }

  void setParam(int index, const CBVar &value) {
    switch (index) {
    case 0: {
      _name = value.payload.stringValue;

      // destroy if we had a block already
      if (_wrapped)
        _wrapped->destroy(_wrapped);
      // create the block directly here
      _wrapped = createBlock(_name.c_str());
      // and setup if successful
      if (_wrapped) {
        _wrapped->setup(_wrapped);
      }
    } break;
    case 1: {
      _wrappedParams.clear();
      for (auto &v : value) {
        _wrappedParams.emplace_back(v);
      }
    } break;
    default:
      break;
    }
  }

  CBVar getParam(int index) {
    switch (index) {
    case 0:
      return Var(_name);
    case 1: {
      CBVar res{};
      res.valueType = Seq;
      const auto nparams = _wrappedParams.size();
      res.payload.seqValue.elements =
          nparams > 0 ? &_wrappedParams.front() : nullptr;
      res.payload.seqValue.len = nparams;
      res.payload.seqValue.cap = 0;
      return res;
    }
    default:
      return {};
    }
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    if (!_wrapped)
      return {};

    // set the wrapped params here
    auto params = _wrapped->parameters(_wrapped);
    for (uint32_t i = 0; i < params.len; i++) {
      if (!validateSetParam(
              _wrapped, i, _wrappedParams[i],
              [](const CBlock *errorBlock, const char *errorTxt,
                 bool nonfatalWarning, void *userData) {},
              nullptr))
        throw CBException(
            "Failed to validate a parameter within a wrapped DBlock.");
      _wrapped->setParam(_wrapped, int(i), &_wrappedParams[i]);
    }
    // and compose finally
    if (_wrapped->compose) {
      return _wrapped->compose(_wrapped, data);
    } else {
      // need to return something valid following runtime validation
      auto outputTypes = _wrapped->outputTypes(_wrapped);
      if (outputTypes.len == 1 &&
          outputTypes.elements[0].basicType != CBType::Any) {
        return outputTypes.elements[0];
      } else {
        return {};
      }
    }
  }

  void destroy() {
    if (_wrapped)
      _wrapped->destroy(_wrapped);
  }

  CBTypesInfo inputTypes() {
    if (_wrapped)
      return _wrapped->inputTypes(_wrapped);
    else
      return CoreInfo::NoneType;
  }

  CBTypesInfo outputTypes() {
    if (_wrapped)
      return _wrapped->outputTypes(_wrapped);
    else
      return CoreInfo::NoneType;
  }

  CBExposedTypesInfo exposedVariables() {
    if (_wrapped)
      return _wrapped->exposedVariables(_wrapped);
    else
      return {};
  }

  CBExposedTypesInfo requiredVariables() {
    if (_wrapped)
      return _wrapped->requiredVariables(_wrapped);
    else
      return {};
  }

  void warmup(CBContext *context) {
    if (_wrapped && _wrapped->warmup) // it's optional!
      _wrapped->warmup(_wrapped, context);
  }

  void cleanup() {
    if (_wrapped)
      _wrapped->cleanup(_wrapped);
  }

  CBVar activate(CBContext *context, const CBVar &input) {
    if (!_wrapped) {
      throw ActivationError("Wrapped block was null!");
    }

    return _wrapped->activate(_wrapped, context, &input);
  }

  void mutate(CBTable options) {
    if (_wrapped && _wrapped->mutate)
      _wrapped->mutate(_wrapped, options);
  }

  void crossover(CBVar state0, CBVar state1) {
    if (_wrapped && _wrapped->crossover)
      _wrapped->crossover(_wrapped, &state0, &state1);
  }

  CBVar getState() {
    if (_wrapped && _wrapped->getState)
      return _wrapped->getState(_wrapped);
    else
      return {};
  }

  void setState(CBVar state) {
    if (_wrapped && _wrapped->setState)
      _wrapped->setState(_wrapped, &state);
  }

  void resetState() {
    if (_wrapped && _wrapped->resetState)
      _wrapped->resetState(_wrapped);
  }

private:
  CBlock *_wrapped = nullptr;
  std::string _name;
  std::vector<OwnedVar> _wrappedParams;
};

void registerBlocks() {
  REGISTER_CBLOCK("Evolve", Evolve);
  REGISTER_CBLOCK("Mutant", Mutant);
  REGISTER_CBLOCK("DBlock", DBlock);
}
} // namespace Genetic
} // namespace chainblocks
