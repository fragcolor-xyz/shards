/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2019-2021 Giovanni Petrantoni */

#include "../edn/print.hpp"
#include "../edn/read.hpp"
#include "shared.hpp"

namespace chainblocks {
namespace edn {
struct Uglify {
  std::string _output;
  static CBTypesInfo inputTypes() { return CoreInfo::StringType; }
  static CBTypesInfo outputTypes() { return CoreInfo::StringType; }
  CBVar activate(CBContext *context, const CBVar &input) {
    const auto s =
        input.payload.stringLen > 0
            ? std::string(input.payload.stringValue, input.payload.stringLen)
            : std::string(input.payload.stringValue);
    _output.assign(print(read(s)));
    return Var(_output);
  }
};

void registerBlocks() { REGISTER_CBLOCK("EDN.Uglify", Uglify); }
} // namespace edn
} // namespace chainblocks