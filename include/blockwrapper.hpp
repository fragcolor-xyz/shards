/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019 Giovanni Petrantoni */

#pragma once

#include "chainblocks.h"
#include "nameof.hpp"

namespace chainblocks {
// SFINAE tests
#define CBLOCK_SFINAE_TEST(_name_)                                             \
  template <typename T> class has_##_name_ {                                   \
    typedef char one;                                                          \
    struct two {                                                               \
      char x[2];                                                               \
    };                                                                         \
    template <typename C> static one test(typeof(&C::_name_));                 \
    template <typename C> static two test(...);                                \
                                                                               \
  public:                                                                      \
    enum { value = sizeof(test<T>(0)) == sizeof(char) };                       \
  }

CBLOCK_SFINAE_TEST(name);
CBLOCK_SFINAE_TEST(help);
CBLOCK_SFINAE_TEST(setup);
CBLOCK_SFINAE_TEST(destroy);
CBLOCK_SFINAE_TEST(inputTypes);
CBLOCK_SFINAE_TEST(outputTypes);
CBLOCK_SFINAE_TEST(exposedVariables);
CBLOCK_SFINAE_TEST(consumedVariables);
CBLOCK_SFINAE_TEST(inferTypes);
CBLOCK_SFINAE_TEST(parameters);
CBLOCK_SFINAE_TEST(setParam);
CBLOCK_SFINAE_TEST(getParam);
CBLOCK_SFINAE_TEST(activate);
CBLOCK_SFINAE_TEST(cleanup);

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
        reinterpret_cast<BlockWrapper<T> *>(b)->block.destroy();
      });
    } else {
      result->destroy = static_cast<CBDestroyProc>([](CBlock *b) {});
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
