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
CBLOCK_SFINAE_TEST(exposedVariables);
CBLOCK_SFINAE_TEST(consumedVariables);
CBLOCK_SFINAE_TEST(inferTypes);
CBLOCK_SFINAE_TEST(parameters);
CBLOCK_SFINAE_TEST(setParam);
CBLOCK_SFINAE_TEST(getParam);
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
        return reinterpret_cast<BlockWrapper<T> *>(b)->block.setup();
      });
    } else {
      result->setup = static_cast<CBSetupProc>([](CBlock *b) {});
    }

    return result;
  }
};
}; // namespace chainblocks
