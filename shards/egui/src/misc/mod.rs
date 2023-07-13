/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use shards::core::registerShard;
use shards::types::ExposedTypes;
use shards::types::ParamVar;

struct Reset {
  parents: ParamVar,
  requiring: ExposedTypes,
}

struct AddFont {
  instance: ParamVar,
  requiring: ExposedTypes,
}

struct Style {
  instance: ParamVar,
  parents: ParamVar,
  requiring: ExposedTypes,
}

mod add_font;
mod reset;
mod style;
pub(crate) mod style_util;

pub fn registerShards() {
  registerShard::<Reset>();
  registerShard::<Style>();
  registerShard::<AddFont>();
}
