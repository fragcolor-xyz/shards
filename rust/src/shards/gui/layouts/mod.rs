/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::core::registerShard;
use crate::types::ExposedTypes;
use crate::types::ParamVar;
use crate::types::ShardsVar;

struct CollapsingHeader {
  parents: ParamVar,
  requiring: ExposedTypes,
  heading: ParamVar,
  contents: ShardsVar,
  defaultOpen: ParamVar,
}

struct Group {
  parents: ParamVar,
  requiring: ExposedTypes,
  contents: ShardsVar,
  exposing: ExposedTypes,
}

struct Horizontal {
  parents: ParamVar,
  requiring: ExposedTypes,
  contents: ShardsVar,
  wrap: ParamVar,
  exposing: ExposedTypes,
}

struct ScrollArea {
  parents: ParamVar,
  requiring: ExposedTypes,
  contents: ShardsVar,
  horizontal: ParamVar,
  vertical: ParamVar,
  exposing: ExposedTypes,
}

struct Separator {
  parents: ParamVar,
  requiring: ExposedTypes,
}

struct Vertical {
  parents: ParamVar,
  requiring: ExposedTypes,
  contents: ShardsVar,
  exposing: ExposedTypes,
}

mod collapsing_header;
mod group;
mod horizontal;
mod scroll_area;
mod separator;
mod vertical;

pub fn registerShards() {
  registerShard::<CollapsingHeader>();
  registerShard::<Group>();
  registerShard::<Horizontal>();
  registerShard::<ScrollArea>();
  registerShard::<Separator>();
  registerShard::<Vertical>();
}
