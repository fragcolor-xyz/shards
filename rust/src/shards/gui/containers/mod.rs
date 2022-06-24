/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::core::registerShard;
use crate::types::ExposedTypes;
use crate::types::ParamVar;
use crate::types::ShardsVar;

struct Panels {
  instance: ParamVar, // Context parameter, this will go will with trait system (users able to plug into existing UIs and interop with them)
  requiring: ExposedTypes,
  top: ShardsVar,
  left: ShardsVar,
  center: ShardsVar,
  right: ShardsVar,
  bottom: ShardsVar,
  ui_ctx_instance: ParamVar,
}

/// Standalone window.
struct Window {
  instance: ParamVar, // Context parameter, this will go will with trait system (users able to plug into existing UIs and interop with them)
  requiring: ExposedTypes,
  title: ParamVar,
  contents: ShardsVar,
  ui_ctx_instance: ParamVar,
}

mod panels;
mod window;

pub fn registerShards() {
  registerShard::<Panels>();
  registerShard::<Window>();
}
