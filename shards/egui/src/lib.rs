/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

// Required for shards-egui C++ bindings
#![cfg_attr(all(target_os = "windows", target_arch = "x86"), feature(abi_thiscall))]

use crate::layouts::LAYOUT_FRAME_TYPE;
use shards::core::cloneVar;
use shards::core::register_legacy_shard;
use shards::core::register_legacy_enum;
use shards::fourCharacterCode;
use shards::shard::LegacyShard;
use shards::shardsc;
use shards::types::common_type;
use shards::types::ExposedTypes;
use shards::types::OptionalString;
use shards::types::ParamVar;
use shards::types::ShardsVar;
use shards::types::Type;
use shards::types::Var;
use shards::types::FRAG_CC;
use shards::SHObjectTypeInfo;
use std::ffi::c_void;
use std::ffi::CStr;
use std::ffi::CString;

#[macro_use]
extern crate shards;

#[macro_use]
extern crate lazy_static;

pub static ANY_TABLE_SLICE: &[Type] = &[common_type::any_table, common_type::any_table_var];
pub static ANY_VAR_SLICE: &[Type] = &[common_type::any, common_type::any_var];
pub static COLOR_VAR_OR_NONE_SLICE: &[Type] = &[
  common_type::color,
  common_type::color_var,
  common_type::none,
];
pub static FLOAT_VAR_SLICE: &[Type] = &[common_type::float, common_type::float_var];
pub static FLOAT_VAR_OR_NONE_SLICE: &[Type] = &[
  common_type::float,
  common_type::float_var,
  common_type::none,
];
pub static FLOAT2_VAR_SLICE: &[Type] = &[common_type::float2, common_type::float2_var];
pub static HASH_VAR_OR_NONE_SLICE: &[Type] = &[common_type::none, common_type::int2_var];
pub static INT_VAR_OR_NONE_SLICE: &[Type] =
  &[common_type::int, common_type::int_var, common_type::none];
pub static STRING_VAR_SLICE: &[Type] = &[common_type::string, common_type::string_var];
pub static STRING_OR_SHARDS_OR_NONE_TYPES_SLICE: &[Type] = &[
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
  static ref LAYOUTCLASS_TYPE: Type = {
    let mut t = common_type::object;
    t.details.object = SHObjectTypeInfo {
      vendorId: FRAG_CC, // 'frag'
      typeId: 0x6C61796F, // 'layo'
    };
    t
  };
  static ref GFX_CONTEXT_TYPE: Type =
    unsafe { *(bindings::gfx_getGraphicsContextType() as *mut shardsc::SHTypeInfo) };
  static ref WINDOW_CONTEXT_TYPE: Type =
    unsafe { *(bindings::gfx_getWindowContextType() as *mut shardsc::SHTypeInfo) };
  static ref INPUT_CONTEXT_TYPE: Type =
    unsafe { *(bindings::gfx_getInputContextType() as *mut shardsc::SHTypeInfo) };
  static ref GFX_QUEUE_TYPE: Type =
    unsafe { *(bindings::gfx_getQueueType() as *mut shardsc::SHTypeInfo) };
  static ref GFX_QUEUE_TYPES: Vec<Type> = vec![*GFX_QUEUE_TYPE];
  static ref GFX_QUEUE_VAR: Type = Type::context_variable(&GFX_QUEUE_TYPES);
  static ref GFX_QUEUE_VAR_TYPES: Vec<Type> = vec![*GFX_QUEUE_VAR];
  static ref LAYOUTCLASS_TYPE_VEC: Vec<Type> = vec![*LAYOUTCLASS_TYPE];
  static ref LAYOUTCLASS_VAR_TYPE: Type = Type::context_variable(&LAYOUTCLASS_TYPE_VEC);
  static ref LAYOUTCLASS_TYPE_VEC_VAR: Vec<Type> = vec![*LAYOUTCLASS_VAR_TYPE];
  static ref LAYOUT_FRAME_TYPE_VEC: Vec<Type> = vec![*LAYOUT_FRAME_TYPE];
  static ref LAYOUT_FRAME_VAR_TYPE: Type = Type::context_variable(&LAYOUT_FRAME_TYPE_VEC);
  static ref LAYOUT_FRAME_TYPE_VEC_VAR: Vec<Type> = vec![*LAYOUT_FRAME_VAR_TYPE];
}

lazy_static! {
  static ref HELP_OUTPUT_EQUAL_INPUT: OptionalString =
    OptionalString(shccstr!("The output of this shard will be its input."));
  static ref HELP_VALUE_IGNORED: OptionalString = OptionalString(shccstr!("The value is ignored."));
}

pub const CONTEXTS_NAME: &str = "UI.Contexts";
pub const PARENTS_UI_NAME: &str = "UI.Parents";
lazy_static! {
  pub static ref CONTEXTS_NAME_CSTR: CString = CString::new(CONTEXTS_NAME).unwrap();
  pub static ref PARENTS_UI_NAME_CSTR: CString = CString::new(PARENTS_UI_NAME).unwrap();
}

#[derive(Hash)]
struct EguiId {
  p: usize,
  idx: u8,
}

impl EguiId {
  fn new<T>(shard: &T, idx: u8) -> EguiId {
    EguiId {
      p: shard as *const T as *mut c_void as usize,
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
pub mod util;
pub mod widgets;

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

shenum! {
  pub struct Anchor {
    [description("Top left corner.")]
    const TopLeft = 0x00;
    [description("Middle left.")]
    const Left = 0x10;
    [description("Bottom left corner.")]
    const BottomLeft = 0x20;
    [description("Top middle.")]
    const Top = 0x01;
    [description("Center.")]
    const Center = 0x11;
    [description("Bottom middle.")]
    const Bottom = 0x21;
    [description("Top right corner.")]
    const TopRight = 0x02;
    [description("Middle right.")]
    const Right = 0x12;
    [description("Bottom right corner.")]
    const BottomRight = 0x22;
  }

  pub struct AnchorInfo {}
}

shenum_types! {
  AnchorInfo,
  const AnchorCC = fourCharacterCode(*b"egAn");
  pub static ref AnchorEnumInfo;
  pub static ref ANCHOR_TYPE: Type;
  pub static ref ANCHOR_TYPES: Vec<Type>;
  pub static ref SEQ_OF_ANCHOR: Type;
  pub static ref SEQ_OF_ANCHOR_TYPES: Vec<Type>;
}

impl TryFrom<Anchor> for egui::Align2 {
  type Error = &'static str;

  fn try_from(value: Anchor) -> Result<Self, Self::Error> {
    match value {
      Anchor::TopLeft => Ok(egui::Align2::LEFT_TOP),
      Anchor::Left => Ok(egui::Align2::LEFT_CENTER),
      Anchor::BottomLeft => Ok(egui::Align2::LEFT_BOTTOM),
      Anchor::Top => Ok(egui::Align2::CENTER_TOP),
      Anchor::Center => Ok(egui::Align2::CENTER_CENTER),
      Anchor::Bottom => Ok(egui::Align2::CENTER_BOTTOM),
      Anchor::TopRight => Ok(egui::Align2::RIGHT_TOP),
      Anchor::Right => Ok(egui::Align2::RIGHT_CENTER),
      Anchor::BottomRight => Ok(egui::Align2::RIGHT_BOTTOM),
      _ => Err("Invalid value for Anchor"),
    }
  }
}

shenum! {
  pub struct Order {
    [description("Painted behind all floating windows.")]
    const Background = 0;
    [description("Special layer between panels and windows.")]
    const PanelResizeLine = 1;
    [description("Normal moveable windows that you reorder by click.")]
    const Middle = 2;
    [description("Popups, menus etc that should always be painted on top of windows. Foreground objects can also have tooltips.")]
    const Foreground = 3;
    [description("Things floating on top of everything else, like tooltips. You cannot interact with these.")]
    const Tooltip = 4;
    [description("Debug layer, always painted last / on top.")]
    const Debug = 5;
  }

  pub struct OrderInfo {}
}

shenum_types! {
  OrderInfo,
  const OrderCC = fourCharacterCode(*b"egOr");
  pub static ref OrderEnumInfo;
  pub static ref ORDER_TYPE: Type;
  pub static ref ORDER_TYPES: Vec<Type>;
  pub static ref SEQ_OF_ORDER: Type;
  pub static ref SEQ_OF_ORDER_TYPES: Vec<Type>;
}

impl TryFrom<Order> for egui::Order {
  type Error = &'static str;

  fn try_from(value: Order) -> Result<Self, Self::Error> {
    match value {
      Order::Background => Ok(egui::Order::Background),
      Order::PanelResizeLine => Ok(egui::Order::PanelResizeLine),
      Order::Middle => Ok(egui::Order::Middle),
      Order::Foreground => Ok(egui::Order::Foreground),
      Order::Tooltip => Ok(egui::Order::Tooltip),
      Order::Debug => Ok(egui::Order::Debug),
      _ => Err("Invalid value for Order"),
    }
  }
}

pub extern "C" fn register(core: *mut shards::shardsc::SHCore) {
  unsafe {
    shards::core::Core = core;
  }

  register_legacy_enum(FRAG_CC, AnchorCC, AnchorEnumInfo.as_ref().into());
  register_legacy_enum(FRAG_CC, OrderCC, OrderEnumInfo.as_ref().into());
  context::register_shards();
  state::register_shards();
  containers::register_shards();
  layouts::register_shards();
  menus::register_shards();
  misc::register_shards();
  widgets::register_shards();
  properties::register_shards();
}
