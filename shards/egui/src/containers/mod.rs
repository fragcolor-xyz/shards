/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use shards::core::register_enum;
use shards::core::register_legacy_enum;
use shards::core::register_legacy_shard;
use shards::core::register_shard;
use shards::fourCharacterCode;
use shards::types::ExposedTypes;
use shards::types::ParamVar;
use shards::types::Type;
use shards::types::FRAG_CC;
use shards::types::{ShardsVar, Table, Var};

shenum! {
  pub struct Order {
  [description("Painted behind all floating windows.")]
  const Background = 1 << 0;
  [description("Special layer between panels and windows.")]
  const PanelResizeLine = 1 << 1;
  [description("Normal moveable windows that you reorder by click.")]
  const Middle = 1 << 2;
  [description("Popups, menus etc that should always be painted on top of windows. Foreground objects can also have tooltips.")]
  const Foreground = 1 << 3;
  [description("Things floating on top of everything else, like tooltips. You cannot interact with these.")]
  const Tooltip = 1 << 4;
  [description("Debug layer, always painted last / on top.")]
  const Debug = 1 << 5;
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

impl From<Order> for egui::Order {
  fn from(order: Order) -> Self {
    match order {
      Order::Background => egui::Order::Background,
      Order::PanelResizeLine => egui::Order::PanelResizeLine,
      Order::Middle => egui::Order::Middle,
      Order::Foreground => egui::Order::Foreground,
      Order::Tooltip => egui::Order::Tooltip,
      Order::Debug => egui::Order::Debug,
      _ => unreachable!(),
    }
  }
}

struct DockArea {
  instance: ParamVar,
  requiring: ExposedTypes,
  contents: ParamVar,
  parents: ParamVar,
  exposing: ExposedTypes,
  headers: Vec<ParamVar>,
  shards: Vec<ShardsVar>,
  tabs: egui_dock::DockState<(ParamVar, ShardsVar)>,
}

#[derive(shards::shards_enum)]
#[enum_info(
  b"egPL",
  "PopupLocation",
  "Determines the location of the popup relative to the widget that it is attached to."
)]
pub enum PopupLocation {
  #[enum_value("Below.")]
  Below = 0,
  #[enum_value("Above.")]
  Above = 1,
}

impl From<PopupLocation> for egui::AboveOrBelow {
  fn from(popup_location: PopupLocation) -> Self {
    match popup_location {
      PopupLocation::Above => egui::AboveOrBelow::Above,
      PopupLocation::Below => egui::AboveOrBelow::Below,
      _ => unreachable!(),
    }
  }
}

struct Scope {
  parents: ParamVar,
  requiring: ExposedTypes,
  contents: ShardsVar,
  exposing: ExposedTypes,
}

struct Tab {
  parents: ParamVar,
  requiring: ExposedTypes,
  title: ParamVar,
  contents: ShardsVar,
  exposing: ExposedTypes,
}

shenum! {
  pub struct WindowFlags {
    [description("Do not display the title bar.")]
    const NoTitleBar = 1 << 0;
    [description("Do not allow resizing the window.")]
    const NoResize = 1 << 1;
    [description("Display scrollbars.")]
    const Scrollbars = 1 << 2;
    [description("Do not display the collapse button.")]
    const NoCollapse = 1 << 3;
    [description("Do not allow window movement.")]
    const Immovable = 1 << 4;
  }
  pub struct WindowFlagsInfo {}
}

shenum_types! {
  WindowFlagsInfo,
  const WindowFlagsCC = fourCharacterCode(*b"egWF");
  pub static ref WindowFlagsEnumInfo;
  pub static ref WINDOW_FLAGS_TYPE: Type;
  pub static ref WINDOW_FLAGS_TYPES: Vec<Type>;
  pub static ref SEQ_OF_WINDOW_FLAGS: Type;
  pub static ref SEQ_OF_WINDOW_FLAGS_TYPES: Vec<Type>;
}

macro_rules! decl_panel {
  ($name:ident, $default_size:ident, $min_size:ident, $max_size:ident) => {
    struct $name {
      instance: ParamVar,
      requiring: ExposedTypes,
      resizable: ParamVar,
      $default_size: ParamVar,
      $min_size: ParamVar,
      $max_size: ParamVar,
      contents: ShardsVar,
      parents: ParamVar,
      exposing: ExposedTypes,
    }
  };
}

decl_panel!(BottomPanel, default_height, min_height, max_height);
decl_panel!(LeftPanel, default_width, min_width, max_width);
decl_panel!(RightPanel, default_width, min_width, max_width);
decl_panel!(TopPanel, default_height, min_height, max_height);

struct CentralPanel {
  instance: ParamVar,
  requiring: ExposedTypes,
  contents: ShardsVar,
  parents: ParamVar,
  exposing: ExposedTypes,
}

mod area;
mod docking;
mod overlay;
mod panels;
mod popup_wrapper;
mod scope;
mod selectable;
mod window;

pub fn register_shards() {
  area::register_shards();
  docking::register_shards();
  window::register_shards();
  popup_wrapper::register_shards();
  selectable::register_shards();
  register_enum::<PopupLocation>();
  register_legacy_shard::<Scope>();
  register_legacy_enum(FRAG_CC, WindowFlagsCC, WindowFlagsEnumInfo.as_ref().into());
  register_legacy_shard::<BottomPanel>();
  register_legacy_shard::<CentralPanel>();
  register_legacy_shard::<LeftPanel>();
  register_legacy_shard::<RightPanel>();
  register_legacy_shard::<TopPanel>();
  register_shard::<overlay::OverlayShard>();

  assert_eq!(WindowFlagsCC, 1701271366);
}

// lazy static table with experimental true set
lazy_static! {
  static ref EXPERIMENTAL_TRUE: Table = {
    let mut table = Table::new();
    let key = Var::ephemeral_string("experimental");
    let value: Var = true.into();
    table.insert(key, &value);
    table
  };
}
