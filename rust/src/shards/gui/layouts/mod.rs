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

mod horizontal;

pub fn registerShards() {
  registerShard::<Horizontal>();
}
