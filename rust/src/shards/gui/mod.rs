/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::core::registerShard;
use crate::shard::Shard;
use crate::types::common_type;
use crate::types::ParamVar;
use crate::types::ShardsVar;
use crate::types::Type;
use crate::types::FRAG_CC;
use egui::Context as EguiNativeContext;
use std::ffi::c_void;

static BOOL_OR_NONE_SLICE: &[Type] = &[common_type::bool, common_type::none];

static EGUI_UI_TYPE: Type = Type::object(FRAG_CC, 1701279061); // 'eguU'
static EGUI_UI_SLICE: &'static [Type] = &[EGUI_UI_TYPE];
static EGUI_UI_SEQ_TYPE: Type = Type::seq(EGUI_UI_SLICE);

static EGUI_CTX_TYPE: Type = Type::object(FRAG_CC, 1701279043); // 'eguC'
static EGUI_CTX_SLICE: &'static [Type] = &[EGUI_CTX_TYPE];
static EGUI_CTX_VAR: Type = Type::context_variable(EGUI_CTX_SLICE);
static EGUI_CTX_VAR_TYPES: &'static [Type] = &[EGUI_CTX_VAR];

const CONTEXT_NAME: &'static str = "UI.Context";
const PARENTS_UI_NAME: &'static str = "UI.Parents";

#[derive(Hash)]
struct EguiId {
  p: usize,
  idx: u8,
}

impl EguiId {
  fn new(shard: &dyn Shard, idx: u8) -> EguiId {
    EguiId {
      p: shard as *const dyn Shard as *mut c_void as usize,
      idx,
    }
  }
}

struct EguiContext {
  context: Option<EguiNativeContext>,
  instance: ParamVar,
  contents: ShardsVar,
}

mod containers;
mod context;
mod widgets;

pub fn registerShards() {
  containers::registerShards();
  registerShard::<EguiContext>();
  widgets::registerShards();
}
