/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::core::registerShard;
use crate::types::common_type;
use crate::types::ExposedTypes;
use crate::types::ParamVar;
use crate::types::ShardsVar;
use crate::types::Type;
use crate::types::Var;

static FLOAT2_VAR_SLICE: &[Type] = &[common_type::float2, common_type::float2_var];

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

struct Hyperlink {
  parents: ParamVar,
  requiring: ExposedTypes,
  label: ParamVar,
}

struct Image {
  parents: ParamVar,
  requiring: ExposedTypes,
  scale: ParamVar,
  texture: Option<egui::TextureHandle>,
}

/// Clickable button with an image.
struct ImageButton {
  parents: ParamVar,
  requiring: ExposedTypes,
  action: ShardsVar,
  scale: ParamVar,
  selected: ParamVar,
  exposing: ExposedTypes,
  should_expose: bool,
  texture: Option<egui::TextureHandle>,
}

/// Displays text.
struct Label {
  parents: ParamVar,
  requiring: ExposedTypes,
  wrap: ParamVar,
}

struct ProgressBar {
  parents: ParamVar,
  requiring: ExposedTypes,
  overlay: ParamVar,
}

/// Radio button with a text label.
struct RadioButton {
  parents: ParamVar,
  requiring: ExposedTypes,
  label: ParamVar,
  variable: ParamVar,
  value: Var,
  exposing: ExposedTypes,
  should_expose: bool,
}

struct Spinner {
  parents: ParamVar,
  requiring: ExposedTypes,
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

macro_rules! decl_ui_input {
  ($name:ident, $tmp_type:ty) => {
    struct $name {
      parents: ParamVar,
      requiring: ExposedTypes,
      variable: ParamVar,
      exposing: ExposedTypes,
      should_expose: bool,
      tmp: $tmp_type,
    }
  };
}

decl_ui_input!(FloatInput, f64);
decl_ui_input!(Float2Input, [f64; 2]);
decl_ui_input!(Float3Input, [f32; 3]);
decl_ui_input!(Float4Input, [f32; 4]);
decl_ui_input!(IntInput, i64);
decl_ui_input!(Int2Input, [i64; 2]);
decl_ui_input!(Int3Input, [i32; 3]);
decl_ui_input!(Int4Input, [i32; 4]);

macro_rules! decl_ui_slider {
  ($name:ident, $tmp_type:ty) => {
    struct $name {
      parents: ParamVar,
      requiring: ExposedTypes,
      label: ParamVar,
      variable: ParamVar,
      min: ParamVar,
      max: ParamVar,
      exposing: ExposedTypes,
      should_expose: bool,
      tmp: $tmp_type,
    }
  };
}

decl_ui_slider!(FloatSlider, f64);
decl_ui_slider!(Float2Slider, [f64; 2]);
decl_ui_slider!(Float3Slider, [f32; 3]);
decl_ui_slider!(Float4Slider, [f32; 4]);
decl_ui_slider!(IntSlider, i64);
decl_ui_slider!(Int2Slider, [i64; 2]);
decl_ui_slider!(Int3Slider, [i32; 3]);
decl_ui_slider!(Int4Slider, [i32; 4]);

mod button;
mod checkbox;
mod hyperlink;
mod image;
mod image_button;
mod label;
mod numeric_input;
mod numeric_slider;
mod progress_bar;
mod radio_button;
mod spinner;
mod text_input;

pub fn registerShards() {
  registerShard::<Button>();
  registerShard::<Checkbox>();
  registerShard::<Hyperlink>();
  registerShard::<Image>();
  registerShard::<ImageButton>();
  registerShard::<Label>();
  registerShard::<FloatInput>();
  registerShard::<Float2Input>();
  registerShard::<Float3Input>();
  registerShard::<Float4Input>();
  registerShard::<IntInput>();
  registerShard::<Int2Input>();
  registerShard::<Int3Input>();
  registerShard::<Int4Input>();
  registerShard::<FloatSlider>();
  registerShard::<Float2Slider>();
  registerShard::<Float3Slider>();
  registerShard::<Float4Slider>();
  registerShard::<IntSlider>();
  registerShard::<Int2Slider>();
  registerShard::<Int3Slider>();
  registerShard::<Int4Slider>();
  registerShard::<ProgressBar>();
  registerShard::<RadioButton>();
  registerShard::<Spinner>();
  registerShard::<TextInput>();
}
