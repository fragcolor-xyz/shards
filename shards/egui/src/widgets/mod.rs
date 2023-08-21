/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use egui_memory_editor::MemoryEditor;
use shards::core::registerShard;
use shards::shardsc;

use shards::types::ClonedVar;
use shards::types::ExposedTypes;
use shards::types::ParamVar;
use shards::types::ShardsVar;
use shards::types::Type;
use shards::types::Var;
use std::ops::Range;

mod image_util;

/// Clickable button with a text label.
struct Button {
  parents: ParamVar,
  requiring: ExposedTypes,
  label: ParamVar,
  style: ParamVar,
  action: ShardsVar,
  wrap: ParamVar,
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

struct Image {
  parents: ParamVar,
  requiring: ExposedTypes,
  scale: ParamVar,
  cached_ui_image: image_util::CachedUIImage,
  current_version: u64,
}

struct RenderTarget {
  parents: ParamVar,
  requiring: ExposedTypes,
  scale: ParamVar,
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
  cached_ui_image: image_util::CachedUIImage,
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

struct MarkdownViewer {
  parents: ParamVar,
  requiring: ExposedTypes,
  cache: egui_commonmark::CommonMarkCache,
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

struct TextField {
  parents: ParamVar,
  requiring: ExposedTypes,
  variable: ParamVar,
  multiline: bool,
  exposing: ExposedTypes,
  should_expose: bool,
  mutable_text: bool,
  password: bool,
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
  labeled: bool,
  name: ClonedVar,
  mutable: bool,
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

mod button;
mod checkbox;
// mod code_editor;
mod color_input;
mod combo;
mod console;
mod drag_value;
mod hex_viewer;
mod hyperlink;
mod image;
mod image_button;
mod label;
mod link;
mod listbox;
mod markdown_viewer;
mod numeric_input;
mod numeric_slider;
mod plots;
mod progress_bar;
mod radio_button;
mod render_target;
mod spinner;
mod text_field;
mod text_util;
mod tooltip;
mod var_util;
mod variable;

pub fn registerShards() {
  registerShard::<Button>();
  registerShard::<Checkbox>();
  registerShard::<ColorInput>();
  registerShard::<Combo>();
  registerShard::<Console>();
  registerShard::<Hyperlink>();
  registerShard::<Image>();
  registerShard::<ImageButton>();
  registerShard::<RenderTarget>();
  registerShard::<Label>();
  registerShard::<Link>();
  registerShard::<ListBox>();
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
  plots::registerShards();
  registerShard::<MarkdownViewer>();
  registerShard::<ProgressBar>();
  registerShard::<RadioButton>();
  registerShard::<Spinner>();
  registerShard::<TextField>();
  registerShard::<Tooltip>();
  registerShard::<Variable>();
  registerShard::<WireVariable>();
  // registerShard::<CodeEditor>();
  registerShard::<HexViewer>();
}
