/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::core::registerShard;
use crate::types::ExposedTypes;
use crate::types::ParamVar;
use crate::types::ShardsVar;

/// Clickable button with a text label.
struct Button {
  parents: ParamVar,
  requiring: ExposedTypes,
  label: ParamVar,
  action: ShardsVar,
  wrap: ParamVar,
}

/// Checkbox with a text label.
struct Checkbox {
  parents: ParamVar,
  requiring: ExposedTypes,
  label: ParamVar,
  variable: ParamVar,
  exposing: ExposedTypes,
  should_expose: bool,
}

/// Displays text.
struct Label {
  parents: ParamVar,
  requiring: ExposedTypes,
  wrap: ParamVar,
}

struct TextInput {
  parents: ParamVar,
  requiring: ExposedTypes,
  variable: ParamVar,
  multiline: ParamVar,
  exposing: ExposedTypes,
  should_expose: bool,
  mutable_text: bool,
}

mod button;
mod checkbox;
mod label;
mod text_input;

pub fn registerShards() {
  registerShard::<Button>();
  registerShard::<Checkbox>();
  registerShard::<Label>();
  registerShard::<TextInput>();
}
