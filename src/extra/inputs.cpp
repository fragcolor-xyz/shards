/* SPDX-License-Identifier: BSD 3-Clause "New" or "Revised" License */
/* Copyright © 2021 Giovanni Petrantoni */

#include "./bgfx.hpp"
#include "SDL.h"
#include "blocks/shared.hpp"

namespace chainblocks {
namespace Inputs {
struct Base {
  static inline CBExposedTypeInfo ContextInfo = ExposedInfo::Variable(
      "GFX.CurrentWindow", CBCCSTR("The required SDL window."),
      BGFX::BaseConsumer::windowType);
  static inline ExposedInfo requiredInfo = ExposedInfo(ContextInfo);

  CBExposedTypesInfo requiredVariables() {
    return CBExposedTypesInfo(requiredInfo);
  }

  CBTypeInfo compose(const CBInstanceData &data) {
    if (data.onWorkerThread) {
      throw ComposeError(
          "Inputs Blocks cannot be used on a worker thread (e.g. "
          "within an Await block)");
    }
    return CoreInfo::NoneType; // on purpose to trigger assertion during
                               // validation
  }
};

struct MousePos : public Base {
  static CBTypesInfo inputTypes() { return CoreInfo::NoneType; }
  static CBTypesInfo outputTypes() { return CoreInfo::Int2Type; }

  CBTypeInfo compose(const CBInstanceData &data) { return CoreInfo::Int2Type; }

  CBVar activate(CBContext *context, const CBVar &input) {
    int32_t mouseX, mouseY;
    SDL_GetMouseState(&mouseX, &mouseY);
    return Var(mouseX, mouseY);
  }
};

void registerBlocks() { REGISTER_CBLOCK("Inputs.MousePos", MousePos); }
} // namespace Inputs
} // namespace chainblocks