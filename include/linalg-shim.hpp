/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2021 Giovanni Petrantoni */

#include <linalg.h>

namespace chainblocks {
struct Float4x4 : public linalg::aliases::float4x4 {
  operator CBVar() const {
    CBVar res{};
    res.valueType = CBType::Seq;
    res.payload.seqValue.elements =
        reinterpret_cast<CBVar *>(const_cast<chainblocks::Float4x4 *>(this));
    res.payload.seqValue.len = 4;
    res.payload.seqValue.cap = 0;
    for (auto i = 0; i < 4; i++) {
      res.payload.seqValue.elements[i].valueType = CBType::Float4;
    }
    return res;
  }
};
}; // namespace chainblocks