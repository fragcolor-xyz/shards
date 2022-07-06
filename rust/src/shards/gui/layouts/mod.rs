/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::core::registerShard;
use crate::types::ExposedTypes;
use crate::types::ParamVar;
use crate::types::ShardsVar;

struct Group {
  parents: ParamVar,
  requiring: ExposedTypes,
  contents: ShardsVar,
}

struct Horizontal {
  parents: ParamVar,
  requiring: ExposedTypes,
  contents: ShardsVar,
  wrap: ParamVar,
}

struct ScrollArea {
  parents: ParamVar,
  requiring: ExposedTypes,
  contents: ShardsVar,
  horizontal: ParamVar,
  vertical: ParamVar,
}

struct Separator {
  parents: ParamVar,
  requiring: ExposedTypes,
}

struct Vertical {
  parents: ParamVar,
  requiring: ExposedTypes,
  contents: ShardsVar,
}

mod group;
mod horizontal;
mod scroll_area;
mod separator;
mod vertical;

pub fn registerShards() {
  registerShard::<Group>();
  registerShard::<Horizontal>();
  registerShard::<ScrollArea>();
  registerShard::<Separator>();
  registerShard::<Vertical>();
}
