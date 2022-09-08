/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use crate::core::registerShard;
use crate::fourCharacterCode;
use crate::shard::Shard;
use crate::shardsc;
use crate::types::common_type;
use crate::types::ExposedTypes;
use crate::types::ParamVar;
use crate::types::ShardsVar;
use crate::types::Type;
use crate::types::FRAG_CC;
use egui::Context as EguiNativeContext;
use std::ffi::c_void;

static ANY_VAR_SLICE: &[Type] = &[common_type::any, common_type::any_var];
static BOOL_OR_NONE_SLICE: &[Type] = &[common_type::bool, common_type::none];
static BOOL_VAR_OR_NONE_SLICE: &[Type] =
  &[common_type::bool, common_type::bool_var, common_type::none];
static COLOR_VAR_OR_NONE_SLICE: &[Type] = &[
  common_type::color,
  common_type::color_var,
  common_type::none,
];
static FLOAT2_VAR_SLICE: &[Type] = &[common_type::float2, common_type::float2_var];
static INT_VAR_OR_NONE_SLICE: &[Type] =
  &[common_type::int, common_type::int_var, common_type::none];
static STRING_VAR_SLICE: &[Type] = &[common_type::string, common_type::string_var];
static STRING_OR_SHARDS_OR_NONE_TYPES_SLICE: &[Type] = &[
  common_type::string,
  common_type::shard,
  common_type::shards,
  common_type::none,
];

static EGUI_UI_TYPE: Type = Type::object(FRAG_CC, fourCharacterCode(*b"eguU"));
static EGUI_UI_SLICE: &[Type] = &[EGUI_UI_TYPE];
static EGUI_UI_SEQ_TYPE: Type = Type::seq(EGUI_UI_SLICE);

static EGUI_CTX_TYPE: Type = Type::object(FRAG_CC, fourCharacterCode(*b"eguC"));
static EGUI_CTX_SLICE: &[Type] = &[EGUI_CTX_TYPE];
static EGUI_CTX_SEQ_TYPE: Type = Type::seq(EGUI_CTX_SLICE);

lazy_static! {
  static ref GFX_GLOBALS_TYPE: Type = unsafe { *shardsc::gfx_getMainWindowGlobalsType() };
  static ref GFX_QUEUE_TYPE: Type = unsafe { *shardsc::gfx_getQueueType() };
  static ref GFX_QUEUE_TYPES: Vec<Type> = vec![*GFX_QUEUE_TYPE];
  static ref GFX_QUEUE_VAR: Type = Type::context_variable(&GFX_QUEUE_TYPES);
  static ref GFX_QUEUE_VAR_TYPES: Vec<Type> = vec![*GFX_QUEUE_VAR];
}

const CONTEXT_NAME: &str = "UI.Context";
const PARENTS_UI_NAME: &str = "UI.Parents";

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
  requiring: ExposedTypes,
  queue: ParamVar,
  contents: ShardsVar,
  main_window_globals: ParamVar,
  parents: ParamVar,
  renderer: egui_gfx::Renderer,
  input_translator: egui_gfx::InputTranslator,
}

struct Reset {
  parents: ParamVar,
  requiring: ExposedTypes,
}

mod containers;
mod context;
mod layouts;
mod menus;
mod reset;
mod util;
mod widgets;

pub fn registerShards() {
  containers::registerShards();
  registerShard::<EguiContext>();
  layouts::registerShards();
  menus::registerShards();
  registerShard::<Reset>();
  widgets::registerShards();
}
