/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

// Required for shards-egui C++ bindings
#![cfg_attr(all(target_os = "windows", target_arch = "x86"), feature(abi_thiscall))]

use crate::layouts::LAYOUT_FRAME_TYPE;
use egui::Response;
use shards::core::cloneVar;
use shards::core::register_enum;
use shards::fourCharacterCode;
use shards::shardsc;
use shards::types::common_type;
use shards::types::ClonedVar;
use shards::types::OptionalString;
use shards::types::Type;
use shards::types::Var;
use shards::types::FRAG_CC;
use shards::SHObjectTypeInfo;
use std::cell::RefCell;
use std::ffi::c_void;
use std::ffi::CString;

#[macro_use]
extern crate shards;

#[macro_use]
extern crate lazy_static;

pub static ANY_TABLE_SLICE: &[Type] = &[common_type::any_table, common_type::any_table_var];
pub static ANY_VAR_SLICE: &[Type] = &[common_type::any, common_type::any_var];
pub static BOOL_VAR_SLICE: &[Type] = &[common_type::bool, common_type::bool_var];
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

static EGUI_CTX_TYPE: Type = Type::object(FRAG_CC, fourCharacterCode(*b"eguC"));
static EGUI_CTX_SLICE: &[Type] = &[EGUI_CTX_TYPE];

lazy_static! {
  static ref LAYOUTCLASS_TYPE: Type = {
    let mut t = common_type::object;
    t.details.object = SHObjectTypeInfo {
      vendorId: FRAG_CC, // 'frag'
      typeId: fourCharacterCode(*b"layo")
    };
    t
  };
  static ref GFX_CONTEXT_TYPE: Type =
    unsafe { *(bindings::gfx_getGraphicsContextType() as *mut shardsc::SHTypeInfo) };
  static ref WINDOW_CONTEXT_TYPE: Type =
    unsafe { *(bindings::gfx_getWindowContextType() as *mut shardsc::SHTypeInfo) };
  static ref INPUT_CONTEXT_TYPE: Type =
    unsafe { *(bindings::gfx_getInputContextType() as *mut shardsc::SHTypeInfo) };
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

#[derive(Default)]
pub struct Context {
  pub egui_ctx: egui::Context,
  // Drag and drop value container
  pub dnd_value: RefCell<ClonedVar>,
  pub prev_response: Option<Response>,
  pub override_selection_response: Option<Response>,
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
pub mod dnd;
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

#[derive(shards::shards_enum)]
#[enum_info(b"egAn", "Anchor", "Identifies the anchor point of a UI element.")]
pub enum Anchor {
  #[enum_value("Top left corner.")]
  TopLeft = 0x00,
  #[enum_value("Middle left.")]
  Left = 0x10,
  #[enum_value("Bottom left corner.")]
  BottomLeft = 0x20,
  #[enum_value("Top middle.")]
  Top = 0x01,
  #[enum_value("Center.")]
  Center = 0x11,
  #[enum_value("Bottom middle.")]
  Bottom = 0x21,
  #[enum_value("Top right corner.")]
  TopRight = 0x02,
  #[enum_value("Middle right.")]
  Right = 0x12,
  #[enum_value("Bottom right corner.")]
  BottomRight = 0x22,
}

impl From<Anchor> for egui::Align2 {
  fn from(value: Anchor) -> Self {
    match value {
      Anchor::TopLeft => egui::Align2::LEFT_TOP,
      Anchor::Left => egui::Align2::LEFT_CENTER,
      Anchor::BottomLeft => egui::Align2::LEFT_BOTTOM,
      Anchor::Top => egui::Align2::CENTER_TOP,
      Anchor::Center => egui::Align2::CENTER_CENTER,
      Anchor::Bottom => egui::Align2::CENTER_BOTTOM,
      Anchor::TopRight => egui::Align2::RIGHT_TOP,
      Anchor::Right => egui::Align2::RIGHT_CENTER,
      Anchor::BottomRight => egui::Align2::RIGHT_BOTTOM,
    }
  }
}

#[derive(shards::shards_enum)]
#[enum_info(
  b"egOr",
  "Order",
  "Identifies the order in which UI elements are painted."
)]
pub enum Order {
  #[enum_value("Painted behind all floating windows.")]
  Background = 0,
  #[enum_value("Special layer between panels and windows.")]
  PanelResizeLine = 1,
  #[enum_value("Normal moveable windows that you reorder by click.")]
  Middle = 2,
  #[enum_value("Popups, menus etc that should always be painted on top of windows. Foreground objects can also have tooltips.")]
  Foreground = 3,
  #[enum_value(
    "Things floating on top of everything else, like tooltips. You cannot interact with these."
  )]
  Tooltip = 4,
  #[enum_value("Debug layer, always painted last / on top.")]
  Debug = 5,
}

impl From<Order> for egui::Order {
  fn from(value: Order) -> Self {
    match value {
      Order::Background => egui::Order::Background,
      Order::PanelResizeLine => egui::Order::PanelResizeLine,
      Order::Middle => egui::Order::Middle,
      Order::Foreground => egui::Order::Foreground,
      Order::Tooltip => egui::Order::Tooltip,
      Order::Debug => egui::Order::Debug,
    }
  }
}

pub extern "C" fn register(core: *mut shards::shardsc::SHCore) {
  unsafe {
    shards::core::Core = core;
  }

  register_enum::<Order>();
  register_enum::<Anchor>();
  dnd::register_shards();
  context::register_shards();
  state::register_shards();
  containers::register_shards();
  layouts::register_shards();
  menus::register_shards();
  misc::register_shards();
  widgets::register_shards();
  properties::register_shards();
}
