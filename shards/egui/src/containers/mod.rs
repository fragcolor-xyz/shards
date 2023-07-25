/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use shards::core::registerEnumType;
use shards::core::registerShard;
use shards::fourCharacterCode;
use shards::types::ExposedTypes;
use shards::types::ParamVar;
use shards::types::ShardsVar;
use shards::types::Type;
use shards::types::FRAG_CC;

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

struct Area {
  instance: ParamVar,
  requiring: ExposedTypes,
  position: ParamVar,
  anchor: ParamVar,
  order: ParamVar,
  contents: ShardsVar,
  parents: ParamVar,
  exposing: ExposedTypes,
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

struct DockArea {
  instance: ParamVar,
  requiring: ExposedTypes,
  contents: ParamVar,
  parents: ParamVar,
  exposing: ExposedTypes,
  headers: Vec<ParamVar>,
  shards: Vec<ShardsVar>,
  tabs: egui_dock::Tree<(ParamVar, ShardsVar)>,
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

/// Standalone window.
struct Window {
  instance: ParamVar,
  requiring: ExposedTypes,
  title: ParamVar,
  position: ParamVar,
  anchor: ParamVar,
  width: ParamVar,
  height: ParamVar,
  flags: ParamVar,
  contents: ShardsVar,
  parents: ParamVar,
  id: ParamVar,
  cached_id: Option<egui::Id>,
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
mod panels;
mod scope;
mod window;

pub fn registerShards() {
  registerShard::<Area>();
  registerEnumType(FRAG_CC, OrderCC, OrderEnumInfo.as_ref().into());
  registerEnumType(FRAG_CC, AnchorCC, AnchorEnumInfo.as_ref().into());
  docking::registerShards();
  registerShard::<Scope>();
  registerShard::<Window>();
  registerEnumType(FRAG_CC, WindowFlagsCC, WindowFlagsEnumInfo.as_ref().into());
  registerShard::<BottomPanel>();
  registerShard::<CentralPanel>();
  registerShard::<LeftPanel>();
  registerShard::<RightPanel>();
  registerShard::<TopPanel>();

  assert_eq!(AnchorCC, 1701265774);
  assert_eq!(WindowFlagsCC, 1701271366);
}
