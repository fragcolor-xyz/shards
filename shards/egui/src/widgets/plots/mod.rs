/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use shards::core::register_enum;
use shards::core::register_legacy_shard;
use shards::fourCharacterCode;
use shards::types::ExposedTypes;
use shards::types::ParamVar;
use shards::types::ShardsVar;
use shards::types::Type;
use shards::types::FLOAT2_TYPES;
use shards::types::FRAG_CC;

static EGUI_PLOT_UI_TYPE: Type = Type::object(FRAG_CC, fourCharacterCode(*b"egPC"));

lazy_static! {
  pub static ref SEQ_OF_FLOAT2: Type = Type::seq(&FLOAT2_TYPES);
  pub static ref SEQ_OF_FLOAT2_TYPES: Vec<Type> = vec![*SEQ_OF_FLOAT2];
}

const PLOT_UI_NAME: &'static str = "UI.PlotUI";

struct Plot {
  parents: ParamVar,
  requiring: ExposedTypes,
  contents: ShardsVar,
  view_aspect: ParamVar,
  data_aspect: ParamVar,
  legend: ParamVar,
  plot_context: ParamVar,
  exposing: ExposedTypes,
}

struct PlotBar {
  plot_ui: ParamVar,
  requiring: ExposedTypes,
  color: ParamVar,
  bar_width: ParamVar,
  horizontal: ParamVar,
  name: ParamVar,
}

struct PlotLine {
  plot_ui: ParamVar,
  requiring: ExposedTypes,
  color: ParamVar,
  name: ParamVar,
}

#[derive(shards::shards_enum)]
#[enum_info(b"egMS", "MarkerShape", "Marker shape options for plot points")]
pub enum MarkerShape {
  #[enum_value("Display a point as a circle.")]
  Circle = 0,
  #[enum_value("Display a point as a diamond.")]
  Diamond = 1,
  #[enum_value("Display a point as a square.")]
  Square = 2,
  #[enum_value("Display a point as a cross.")]
  Cross = 3,
  #[enum_value("Display a point as a plus sign.")]
  Plus = 4,
  #[enum_value("Display a point as an arrow pointing upwards.")]
  Up = 5,
  #[enum_value("Display a point as an arrow pointing downwards.")]
  Down = 6,
  #[enum_value("Display a point as an arrow pointing to the left.")]
  Left = 7,
  #[enum_value("Display a point as an arrow pointing to the right.")]
  Right = 8,
  #[enum_value("Display a point as an asterisk.")]
  Asterisk = 9,
}

// egui::widgets::plot::items::MarkerShape

struct PlotPoints {
  plot_ui: ParamVar,
  requiring: ExposedTypes,
  name: ParamVar,
  color: ParamVar,
  shape: ParamVar,
  radius: ParamVar,
}

mod plot;
mod plot_bar;
mod plot_line;
mod plot_points;

pub fn register_shards() {
  register_legacy_shard::<Plot>();
  register_legacy_shard::<PlotBar>();
  register_legacy_shard::<PlotLine>();
  register_legacy_shard::<PlotPoints>();
  register_enum::<MarkerShape>();
}
