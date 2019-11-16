/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

#ifndef CB_BLOCKWRAPPER_HPP
#define CB_BLOCKWRAPPER_HPP

#include "chainblocks.h"
#include "nameof.hpp"
#include "utility.hpp"

namespace chainblocks {
CB_HAS_MEMBER_TEST(name);
CB_HAS_MEMBER_TEST(help);
CB_HAS_MEMBER_TEST(setup);
CB_HAS_MEMBER_TEST(destroy);
CB_HAS_MEMBER_TEST(inputTypes);
CB_HAS_MEMBER_TEST(outputTypes);
CB_HAS_MEMBER_TEST(exposedVariables);
CB_HAS_MEMBER_TEST(consumedVariables);
CB_HAS_MEMBER_TEST(inferTypes);
CB_HAS_MEMBER_TEST(parameters);
CB_HAS_MEMBER_TEST(setParam);
CB_HAS_MEMBER_TEST(getParam);
CB_HAS_MEMBER_TEST(activate);
CB_HAS_MEMBER_TEST(cleanup);

// Composition is preferred
template <class T> struct BlockWrapper {
  CBlock header;
  T block;

  static __cdecl CBlock *create() {
    CBlock *result = reinterpret_cast<CBlock *>(new BlockWrapper<T>());

    // name
    if constexpr (has_name<T>::value) {
      result->name = static_cast<CBNameProc>([](CBlock *b) {
        return reinterpret_cast<BlockWrapper<T> *>(b)->block.name();
      });
    } else {
      result->name = static_cast<CBNameProc>([](CBlock *b) {
        constexpr auto type_name = NAMEOF_TYPE(T);
        return type_name.c_str();
      });
    }

    // help
    if constexpr (has_help<T>::value) {
      result->help = static_cast<CBHelpProc>([](CBlock *b) {
        return reinterpret_cast<BlockWrapper<T> *>(b)->block.help();
      });
    } else {
      result->help = static_cast<CBHelpProc>([](CBlock *b) { return ""; });
    }

    // setup
    if constexpr (has_setup<T>::value) {
      result->setup = static_cast<CBSetupProc>([](CBlock *b) {
        reinterpret_cast<BlockWrapper<T> *>(b)->block.setup();
      });
    } else {
      result->setup = static_cast<CBSetupProc>([](CBlock *b) {});
    }

    // destroy
    if constexpr (has_destroy<T>::value) {
      result->destroy = static_cast<CBDestroyProc>([](CBlock *b) {
        auto bw = reinterpret_cast<BlockWrapper<T> *>(b);
        bw->block.destroy();
        delete bw;
      });
    } else {
      result->destroy = static_cast<CBDestroyProc>([](CBlock *b) {
        auto bw = reinterpret_cast<BlockWrapper<T> *>(b);
        delete bw;
      });
    }

    // inputTypes
    static_assert(has_inputTypes<T>::value,
                  "Blocks must have an \"inputTypes\" method.");
    if constexpr (has_inputTypes<T>::value) {
      result->inputTypes = static_cast<CBInputTypesProc>([](CBlock *b) {
        return reinterpret_cast<BlockWrapper<T> *>(b)->block.inputTypes();
      });
    }

    // outputTypes
    static_assert(has_outputTypes<T>::value,
                  "Blocks must have an \"outputTypes\" method.");
    if constexpr (has_outputTypes<T>::value) {
      result->outputTypes = static_cast<CBOutputTypesProc>([](CBlock *b) {
        return reinterpret_cast<BlockWrapper<T> *>(b)->block.outputTypes();
      });
    }

    // exposedVariables
    if constexpr (has_exposedVariables<T>::value) {
      result->exposedVariables =
          static_cast<CBExposedVariablesProc>([](CBlock *b) {
            return reinterpret_cast<BlockWrapper<T> *>(b)
                ->block.exposedVariables();
          });
    } else {
      result->exposedVariables = static_cast<CBExposedVariablesProc>(
          [](CBlock *b) { return CBExposedTypesInfo(); });
    }

    // consumedVariables
    if constexpr (has_consumedVariables<T>::value) {
      result->consumedVariables =
          static_cast<CBConsumedVariablesProc>([](CBlock *b) {
            return reinterpret_cast<BlockWrapper<T> *>(b)
                ->block.consumedVariables();
          });
    } else {
      result->consumedVariables = static_cast<CBConsumedVariablesProc>(
          [](CBlock *b) { return CBExposedTypesInfo(); });
    }

    // parameters
    if constexpr (has_parameters<T>::value) {
      result->parameters = static_cast<CBParametersProc>([](CBlock *b) {
        return reinterpret_cast<BlockWrapper<T> *>(b)->block.parameters();
      });
    } else {
      result->parameters = static_cast<CBParametersProc>(
          [](CBlock *b) { return CBParametersInfo(); });
    }

    // setParam
    if constexpr (has_setParam<T>::value) {
      result->setParam =
          static_cast<CBSetParamProc>([](CBlock *b, int i, CBVar v) {
            reinterpret_cast<BlockWrapper<T> *>(b)->block.setParam(i, v);
          });
    } else {
      result->setParam =
          static_cast<CBSetParamProc>([](CBlock *b, int i, CBVar v) {});
    }

    // getParam
    if constexpr (has_getParam<T>::value) {
      result->getParam = static_cast<CBGetParamProc>([](CBlock *b, int i) {
        return reinterpret_cast<BlockWrapper<T> *>(b)->block.getParam(i);
      });
    } else {
      result->getParam =
          static_cast<CBGetParamProc>([](CBlock *b, int i) { return CBVar(); });
    }

    // inferTypes
    if constexpr (has_inferTypes<T>::value) {
      result->inferTypes = static_cast<CBInferTypesProc>(
          [](CBlock *b, CBTypeInfo it, CBExposedTypesInfo ci) {
            return reinterpret_cast<BlockWrapper<T> *>(b)->block.inferTypes(it,
                                                                            ci);
          });
    } else {
      result->inferTypes = static_cast<CBInferTypesProc>(
          [](CBlock *b, CBTypeInfo it, CBExposedTypesInfo ci) {
            return CBTypeInfo();
          });
    }

    // activate
    static_assert(has_activate<T>::value,
                  "Blocks must have an \"activate\" method.");
    if constexpr (has_activate<T>::value) {
      result->activate = static_cast<CBActivateProc>(
          [](CBlock *b, CBContext *ctx, const CBVar *v) {
            return reinterpret_cast<BlockWrapper<T> *>(b)->block.activate(ctx,
                                                                          v);
          });
    }

    // cleanup
    if constexpr (has_cleanup<T>::value) {
      result->cleanup = static_cast<CBCleanupProc>([](CBlock *b) {
        reinterpret_cast<BlockWrapper<T> *>(b)->block.cleanup();
      });
    } else {
      result->cleanup = static_cast<CBCleanupProc>([](CBlock *b) {});
    }

    return result;
  }
};
}; // namespace chainblocks

#endif
