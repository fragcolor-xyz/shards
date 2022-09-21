/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::core::registerShard;
use crate::types::ExposedTypes;
use crate::types::ParamVar;

struct Reset {
  parents: ParamVar,
  requiring: ExposedTypes,
}

struct Style {
  parents: ParamVar,
  requiring: ExposedTypes,
}

mod reset;
mod style;

pub fn registerShards() {
  registerShard::<Reset>();
  registerShard::<Style>();
}
