/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::core::registerShard;
use crate::types::ExposedTypes;
use crate::types::ParamVar;
use crate::types::ShardsVar;

struct CollapsingHeader {
  parents: ParamVar,
  requiring: ExposedTypes,
  text: ParamVar,
  header: ShardsVar,
  contents: ShardsVar,
  defaultOpen: ParamVar,
  exposing: ExposedTypes,
}

struct Columns {
  parents: ParamVar,
  requiring: ExposedTypes,
  contents: ParamVar,
  shards: Vec<ShardsVar>,
  exposing: ExposedTypes,
}

struct Disable {
  parents: ParamVar,
  requiring: ExposedTypes,
  contents: ShardsVar,
  disable: ParamVar,
  exposing: ExposedTypes,
}

struct Frame {
  parents: ParamVar,
  requiring: ExposedTypes,
  innerMargin: ParamVar,
  outerMargin: ParamVar,
  rounding: ParamVar,
  fillColor: ParamVar,
  strokeColor: ParamVar,
  strokeWidth: ParamVar,
  contents: ShardsVar,
  exposing: ExposedTypes,
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

struct Indent {
  parents: ParamVar,
  requiring: ExposedTypes,
  contents: ShardsVar,
  exposing: ExposedTypes,
}

struct ScrollArea {
  parents: ParamVar,
  requiring: ExposedTypes,
  contents: ShardsVar,
  horizontal: ParamVar,
  vertical: ParamVar,
  alwaysShow: ParamVar,
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
mod columns;
mod disable;
mod frame;
mod group;
mod horizontal;
mod indent;
mod scroll_area;
mod separator;
mod vertical;

pub fn registerShards() {
  registerShard::<CollapsingHeader>();
  registerShard::<Columns>();
  registerShard::<Disable>();
  registerShard::<Frame>();
  registerShard::<Group>();
  registerShard::<Horizontal>();
  registerShard::<Indent>();
  registerShard::<ScrollArea>();
  registerShard::<Separator>();
  registerShard::<Vertical>();
}
