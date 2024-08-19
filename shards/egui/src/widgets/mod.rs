/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use egui_memory_editor::MemoryEditor;
use image_util::AutoTexturePtr;
use shards::core::register_enum;
use shards::core::register_legacy_shard;
use shards::core::register_shard;
use shards::shardsc;

use shards::types::ClonedVar;
use shards::types::ExposedTypes;
use shards::types::ParamVar;
use shards::types::ShardsVar;
use shards::types::Type;
use shards::types::Var;
use std::ops::Range;

pub mod image_util;

#[derive(shards::shards_enum)]
#[enum_info(b"txtW", "TextWrap", "Text wrapping options for text widgets.")]
pub enum TextWrap {
  #[enum_value("Extends the parent widget's width to wrap text.")]
  Extend = 0x0,
  #[enum_value("Wraps text to the width of the parent widget extending the next line.")]
  Wrap = 0x1,
  #[enum_value("Truncates text that does not fit within the parent widget.")]
  Truncate = 0x2,
}

/// Checkbox with a text label.
struct Checkbox {
  parents: ParamVar,
  requiring: ExposedTypes,
  label: ParamVar,
  style: ParamVar,
  variable: ParamVar,
  exposing: ExposedTypes,
  should_expose: bool,
}

struct CodeEditor {
  parents: ParamVar,
  requiring: ExposedTypes,
  variable: ParamVar,
  language: ParamVar,
  exposing: ExposedTypes,
  should_expose: bool,
  mutable_text: bool,
}

struct ColorInput {
  parents: ParamVar,
  requiring: ExposedTypes,
  variable: ParamVar,
  exposing: ExposedTypes,
  should_expose: bool,
  tmp: shardsc::SHColor,
}

struct Combo {
  parents: ParamVar,
  requiring: ExposedTypes,
  label: ParamVar,
  index: ParamVar,
  width: ParamVar,
  style: ParamVar,
  exposing: ExposedTypes,
  should_expose: bool,
  tmp: usize,
}

struct Console {
  parents: ParamVar,
  requiring: ExposedTypes,
  show_filters: ParamVar,
  style: ParamVar,
  filters: (bool, bool, bool, bool, bool),
}

struct HexViewer {
  parents: ParamVar,
  requiring: ExposedTypes,
  editor: Option<(MemoryEditor, Range<usize>)>,
}

struct Hyperlink {
  parents: ParamVar,
  requiring: ExposedTypes,
  label: ParamVar,
  style: ParamVar,
}

struct RenderTarget {
  contexts: ParamVar,
  parents: ParamVar,
  requiring: ExposedTypes,
  scale: ParamVar,
}

/// Displays text.
struct Label {
  parents: ParamVar,
  requiring: ExposedTypes,
  wrap: ParamVar,
  style: ParamVar,
}

struct Link {
  parents: ParamVar,
  requiring: ExposedTypes,
  label: ParamVar,
  action: ShardsVar,
  style: ParamVar,
}

struct ListBox {
  parents: ParamVar,
  requiring: ExposedTypes,
  index: ParamVar,
  template: ShardsVar,
  is_selected: ShardsVar,
  selected: ShardsVar,
  exposing: ExposedTypes,
  should_expose: bool,
  tmp: i64,
}

struct ProgressBar {
  parents: ParamVar,
  requiring: ExposedTypes,
  overlay: ParamVar,
  desired_width: ParamVar,
}

/// Radio button with a text label.
struct RadioButton {
  parents: ParamVar,
  requiring: ExposedTypes,
  label: ParamVar,
  variable: ParamVar,
  value: Var,
  style: ParamVar,
  exposing: ExposedTypes,
  should_expose: bool,
}

struct Spinner {
  parents: ParamVar,
  requiring: ExposedTypes,
  size: ParamVar,
}

struct Tooltip {
  parents: ParamVar,
  requiring: ExposedTypes,
  contents: ShardsVar,
  text: ParamVar,
  onhover: ShardsVar,
  exposing: ExposedTypes,
}

struct Variable {
  parents: ParamVar,
  requiring: ExposedTypes,
  variable: ParamVar,
  key: ParamVar,
  labeled: bool,
  name: ClonedVar,
  mutable: bool,
  global: bool,
  inner_type: Option<Type>,
}

struct WireVariable {
  parents: ParamVar,
  requiring: ExposedTypes,
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

struct FloatInput {
  parents: ParamVar,
  requiring: ExposedTypes,
  variable: ParamVar,
  prefix: ParamVar,
  exposing: ExposedTypes,
  should_expose: bool,
  tmp: f64,
}
decl_ui_input!(Float2Input, [f64; 2]);
decl_ui_input!(Float3Input, [f32; 3]);
decl_ui_input!(Float4Input, [f32; 4]);
struct IntInput {
  parents: ParamVar,
  requiring: ExposedTypes,
  variable: ParamVar,
  prefix: ParamVar,
  exposing: ExposedTypes,
  should_expose: bool,
  tmp: i64,
}
decl_ui_input!(Int2Input, [i64; 2]);
decl_ui_input!(Int3Input, [i32; 3]);
decl_ui_input!(Int4Input, [i32; 4]);

macro_rules! decl_ui_slider {
  ($name:ident, $tmp_type:ty) => {
    struct $name {
      parents: ParamVar,
      requiring: ExposedTypes,
      label: ParamVar,
      style: ParamVar,
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

pub mod button;
pub mod checkbox;
pub mod code_editor;
pub mod color_input;
pub mod combo;
pub mod console;
pub mod drag_value;
pub mod hex_viewer;
pub mod hyperlink;
pub mod image;
pub mod image_button;
pub mod label;
pub mod link;
pub mod listbox;
pub mod markdown_viewer;
pub mod numeric_input;
pub mod numeric_slider;
pub mod plots;
pub mod progress_bar;
pub mod radio_button;
pub mod render_target;
pub mod spinner;
pub mod text_field;
pub mod text_util;
pub mod tooltip;
pub mod var_util;
pub mod variable;

pub fn register_shards() {
  register_legacy_shard::<Checkbox>();
  register_legacy_shard::<ColorInput>();
  register_legacy_shard::<Combo>();
  register_legacy_shard::<Console>();
  register_legacy_shard::<Hyperlink>();
  register_legacy_shard::<RenderTarget>();
  register_shard::<label::Label>();
  register_legacy_shard::<Link>();
  register_legacy_shard::<ListBox>();
  register_legacy_shard::<FloatInput>();
  register_legacy_shard::<Float2Input>();
  register_legacy_shard::<Float3Input>();
  register_legacy_shard::<Float4Input>();
  register_legacy_shard::<IntInput>();
  register_legacy_shard::<Int2Input>();
  register_legacy_shard::<Int3Input>();
  register_legacy_shard::<Int4Input>();
  register_legacy_shard::<FloatSlider>();
  register_legacy_shard::<Float2Slider>();
  register_legacy_shard::<Float3Slider>();
  register_legacy_shard::<Float4Slider>();
  register_legacy_shard::<IntSlider>();
  register_legacy_shard::<Int2Slider>();
  register_legacy_shard::<Int3Slider>();
  register_legacy_shard::<Int4Slider>();
  button::register_shards();
  plots::register_shards();
  image::register_shards();
  image_button::register_shards();
  register_shard::<markdown_viewer::MarkdownViewerShard>();
  register_legacy_shard::<ProgressBar>();
  register_legacy_shard::<RadioButton>();
  register_legacy_shard::<Spinner>();
  register_shard::<text_field::TextField>();
  register_legacy_shard::<Tooltip>();
  register_legacy_shard::<Variable>();
  register_legacy_shard::<CodeEditor>();
  register_legacy_shard::<HexViewer>();
  register_enum::<TextWrap>();
}
