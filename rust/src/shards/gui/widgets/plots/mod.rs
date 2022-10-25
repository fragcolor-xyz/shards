/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::core::registerShard;
use crate::fourCharacterCode;
use crate::types::ExposedTypes;
use crate::types::FRAG_CC;
use crate::types::ParamVar;
use crate::types::ShardsVar;
use crate::types::Type;

static EGUI_PLOT_UI_TYPE: Type = Type::object(FRAG_CC, fourCharacterCode(*b"egPC"));

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

mod plot;

pub fn registerShards() {
  registerShard::<Plot>();
}
