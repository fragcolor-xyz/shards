/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::core::registerShard;
use crate::types::ExposedTypes;
use crate::types::ParamVar;

struct GetClipboard {
  parents: ParamVar,
  requiring: ExposedTypes,
}

struct SetClipboard {
  parents: ParamVar,
  requiring: ExposedTypes,
}

struct Reset {
  parents: ParamVar,
  requiring: ExposedTypes,
}

mod clipboard;
mod reset;

pub fn registerShards() {
  // registerShard::<GetClipboard>();
  registerShard::<SetClipboard>();
  registerShard::<Reset>();
}
