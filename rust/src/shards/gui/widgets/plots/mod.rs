/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::core::registerShard;
use crate::fourCharacterCode;
use crate::types::ExposedTypes;
use crate::types::FLOAT2_TYPES;
use crate::types::FRAG_CC;
use crate::types::ParamVar;
use crate::types::ShardsVar;
use crate::types::Type;

static EGUI_PLOT_UI_TYPE: Type = Type::object(FRAG_CC, fourCharacterCode(*b"egPC"));

lazy_static!{  
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

mod plot;
mod plot_bar;
mod plot_line;

pub fn registerShards() {
  registerShard::<Plot>();
  registerShard::<PlotBar>();
  registerShard::<PlotLine>();
}
