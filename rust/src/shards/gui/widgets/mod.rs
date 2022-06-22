/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::SHTypeInfo;
use crate::core::registerShard;
use crate::types::ExposedTypes;
use crate::types::ParamVar;
use crate::types::ShardsVar;
use crate::types::common_type;


/// Clickable button with a text label.
struct Button {
  parent: ParamVar,
  requiring: ExposedTypes,
  label: ParamVar,
  action: ShardsVar,
  wrap: ParamVar,
}

/// Displays text.
struct Label {
  parent: ParamVar,
  requiring: ExposedTypes,
  wrap: ParamVar,
}

mod button;
mod label;

pub fn registerShards() {
  registerShard::<Button>();
  registerShard::<Label>();
}
