/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::core::registerShard;
use crate::types::ExposedTypes;
use crate::types::ParamVar;
use crate::types::ShardsVar;

struct Horizontal {
  parents: ParamVar,
  requiring: ExposedTypes,
  contents: ShardsVar,
  wrap: ParamVar,
}

struct Vertical {
  parents: ParamVar,
  requiring: ExposedTypes,
  contents: ShardsVar,
}

mod horizontal;
mod vertical;

pub fn registerShards() {
  registerShard::<Horizontal>();
  registerShard::<Vertical>();
}
