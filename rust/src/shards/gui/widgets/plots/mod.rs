/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::core::registerEnumType;
use crate::core::registerShard;
use crate::fourCharacterCode;
use crate::types::ExposedTypes;
use crate::types::ParamVar;
use crate::types::ShardsVar;
use crate::types::Type;
use crate::types::FLOAT2_TYPES;
use crate::types::FRAG_CC;

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

shenum! {
  struct MarkerShape {
    const Circle = 0;
    const Diamond = 1;
    const Square = 2;
    const Cross = 3;
    const Plus = 4;
    const Up = 5;
    const Down = 6;
    const Left = 7;
    const Right = 8;
    const Asterisk = 9;
  }
  struct MarkerShapeInfo {}
}

shenum_types! {
  MarkerShapeInfo,
  const MarkerShapeCC = fourCharacterCode(*b"egMS");
  static ref MarkerShapeEnumInfo;
  static ref MARKER_SHAPE_TYPE: Type;
  static ref MARKER_SHAPE_TYPES: Vec<Type>;
  static ref SEQ_OF_MARKER_SHAPE: Type;
  static ref SEQ_OF_MARKER_SHAPE_TYPES: Vec<Type>;
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

pub fn registerShards() {
  registerShard::<Plot>();
  registerShard::<PlotBar>();
  registerShard::<PlotLine>();
  registerShard::<PlotPoints>();
  registerEnumType(FRAG_CC, MarkerShapeCC, MarkerShapeEnumInfo.as_ref().into());
}
