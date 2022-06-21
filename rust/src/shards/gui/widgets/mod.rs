/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::core::registerShard;
use crate::types::ExposedTypes;
use crate::types::ParamVar;

/// Displays text.
struct Label {
  parent: ParamVar,
  requiring: ExposedTypes,
  wrap: ParamVar,
}

mod label;

pub fn registerShards() {
  registerShard::<Label>();
}
