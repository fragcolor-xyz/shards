/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

// Required for shards-egui C++ bindings
#![cfg_attr(all(target_os = "windows", target_arch = "x86"), feature(abi_thiscall))]

use shards::core::cloneVar;
use shards::core::registerShard; 
use shards::fourCharacterCode;
use shards::shard::Shard;
use shards::shardsc;
use shards::types::common_type;
use shards::types::ExposedTypes;
use shards::types::OptionalString;
use shards::types::ParamVar;
use shards::types::ShardsVar;
use shards::types::Type;
use shards::types::Var;
use shards::types::FRAG_CC;
use std::ffi::c_void;
use std::ffi::CStr;

#[macro_use]
extern crate shards;

#[macro_use]
extern crate lazy_static;

static ANY_TABLE_SLICE: &[Type] = &[common_type::any_table, common_type::any_table_var];
static ANY_VAR_SLICE: &[Type] = &[common_type::any, common_type::any_var];
static COLOR_VAR_OR_NONE_SLICE: &[Type] = &[
  common_type::color,
  common_type::color_var,
  common_type::none,
];
static FLOAT_VAR_SLICE: &[Type] = &[common_type::float, common_type::float_var];
static FLOAT_VAR_OR_NONE_SLICE: &[Type] = &[
  common_type::float,
  common_type::float_var,
  common_type::none,
];
pub static FLOAT2_VAR_SLICE: &[Type] = &[common_type::float2, common_type::float2_var];
static HASH_VAR_OR_NONE_SLICE: &[Type] = &[common_type::none, common_type::int2_var];
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
  static ref GFX_CONTEXT_TYPE: Type = unsafe { *(bindings::gfx_getGraphicsContextType() as *mut shardsc::SHTypeInfo) };
  static ref WINDOW_CONTEXT_TYPE: Type = unsafe { *(bindings::gfx_getWindowContextType() as *mut shardsc::SHTypeInfo) };
  static ref INPUT_CONTEXT_TYPE: Type = unsafe { *(bindings::gfx_getInputContextType() as *mut shardsc::SHTypeInfo) };
  static ref GFX_QUEUE_TYPE: Type = unsafe { *(bindings::gfx_getQueueType() as *mut shardsc::SHTypeInfo) };
  static ref GFX_QUEUE_TYPES: Vec<Type> = vec![*GFX_QUEUE_TYPE];
  static ref GFX_QUEUE_VAR: Type = Type::context_variable(&GFX_QUEUE_TYPES);
  static ref GFX_QUEUE_VAR_TYPES: Vec<Type> = vec![*GFX_QUEUE_VAR];
}

lazy_static! {
  static ref HELP_OUTPUT_EQUAL_INPUT: OptionalString =
    OptionalString(shccstr!("The output of this shard will be its input."));
  static ref HELP_VALUE_IGNORED: OptionalString = OptionalString(shccstr!("The value is ignored."));
}

const CONTEXTS_NAME: &str = "UI.Contexts";
pub const PARENTS_UI_NAME: &str = "UI.Parents";

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

impl From<EguiId> for egui::Id {
  fn from(value: EguiId) -> Self {
    egui::Id::new(value)
  }
}

mod egui_host;

struct EguiContext {
  host: egui_host::EguiHost,
  requiring: ExposedTypes,
  queue: ParamVar,
  contents: ShardsVar,
  exposing: ExposedTypes,
  has_graphics_context: bool,
  graphics_context: ParamVar,
  input_context: ParamVar,
  renderer: bindings::Renderer,
  input_translator: bindings::InputTranslator,
}
pub(crate) trait UIRenderer {
  fn render(
    &mut self,
    read_only: bool,
    inner_type: Option<&Type>,
    ui: &mut egui::Ui,
  ) -> egui::Response;
}

pub mod bindings;
pub mod containers;
pub mod context;
pub mod layouts;
pub mod menus;
pub mod misc;
pub mod properties;
pub mod state;
pub mod widgets;
pub mod util;

struct VarTextBuffer<'a>(&'a Var);
struct MutVarTextBuffer<'a>(&'a mut Var);

impl<'a> egui::TextBuffer for VarTextBuffer<'a> {
  fn is_mutable(&self) -> bool {
    false
  }

  fn as_str(&self) -> &str {
    self.0.as_ref().try_into().unwrap()
  }

  fn insert_text(&mut self, _text: &str, _char_index: usize) -> usize {
    0usize
  }

  fn delete_char_range(&mut self, _char_range: std::ops::Range<usize>) {}
}

// Warning: this will leak if we ever use this for temporary, the Var needs to be strictly a Shards variable!!!
impl<'a> egui::TextBuffer for MutVarTextBuffer<'a> {
  fn is_mutable(&self) -> bool {
    true
  }

  fn as_str(&self) -> &str {
    self.0.as_ref().try_into().unwrap()
  }

  fn insert_text(&mut self, text: &str, char_index: usize) -> usize {
    let byte_idx = if !self.0.is_string() {
      0usize
    } else {
      self.byte_index_from_char_index(char_index)
    };

    let text_len = text.len();
    let (current_len, current_cap) = unsafe {
      (
        self.0.payload.__bindgen_anon_1.__bindgen_anon_2.stringLen as usize,
        self
          .0
          .payload
          .__bindgen_anon_1
          .__bindgen_anon_2
          .stringCapacity as usize,
      )
    };

    if current_cap == 0usize {
      // new string
      let tmp = Var::ephemeral_string(text);
      cloneVar(self.0, &tmp);
    } else if (current_cap - current_len) >= text_len {
      // text can fit within existing capacity
      unsafe {
        let base_ptr =
          self.0.payload.__bindgen_anon_1.__bindgen_anon_2.stringValue as *mut std::os::raw::c_char;
        // move the rest of the string to the end
        std::ptr::copy(
          base_ptr.add(byte_idx),
          base_ptr.add(byte_idx).add(text_len),
          current_len - byte_idx,
        );
        // insert the new text
        let bytes = text.as_ptr() as *const std::os::raw::c_char;
        std::ptr::copy_nonoverlapping(bytes, base_ptr.add(byte_idx), text_len);
        // update the length
        let new_len = current_len + text_len;
        self.0.payload.__bindgen_anon_1.__bindgen_anon_2.stringLen = new_len as u32;
        // fixup null-terminator
        *base_ptr.add(new_len) = 0;
      }
    } else {
      let mut str = String::from(self.as_str());
      str.insert_str(byte_idx, text);
      let tmp = Var::ephemeral_string(str.as_str());
      cloneVar(self.0, &tmp);
    }

    text.chars().count()
  }

  fn delete_char_range(&mut self, char_range: std::ops::Range<usize>) {
    assert!(char_range.start <= char_range.end);

    let byte_start = self.byte_index_from_char_index(char_range.start);
    let byte_end = self.byte_index_from_char_index(char_range.end);

    if byte_start == byte_end {
      // nothing to do
      return;
    }

    unsafe {
      let current_len = self.0.payload.__bindgen_anon_1.__bindgen_anon_2.stringLen as usize;
      let base_ptr =
        self.0.payload.__bindgen_anon_1.__bindgen_anon_2.stringValue as *mut std::os::raw::c_char;
      // move rest of the text at the deletion location
      std::ptr::copy(
        base_ptr.add(byte_end),
        base_ptr.add(byte_start),
        current_len - byte_end,
      );
      // update the length
      let new_len = current_len - byte_end + byte_start;
      self.0.payload.__bindgen_anon_1.__bindgen_anon_2.stringLen = new_len as u32;
      // fixup null-terminator
      *base_ptr.add(new_len) = 0;
    }
  }
}

pub extern "C" fn register(core: *mut shards::shardsc::SHCore) {
  unsafe {
    shards::core::Core = core;
  }

  registerShard::<EguiContext>();
  state::registerShards();
  containers::registerShards();
  layouts::registerShards();
  menus::registerShards();
  misc::registerShards();
  widgets::registerShards();
  properties::registerShards();
}
