/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

// allow non upper case constants
#![allow(non_upper_case_globals)]

use shards::core::register_enum;
use shards::core::register_legacy_shard;
use shards::core::register_shard;
use shards::types::ExposedTypes;
use shards::types::ParamVar;
use shards::types::WireState;
use shards::types::{ShardsVar, Table, Var};
use shards::Shards;

use crate::util::with_object_stack_var;
use crate::EGUI_UI_TYPE;

#[derive(shards::shards_enum)]
#[enum_info(
  b"egOr",
  "Order",
  "Identifies the order in which UI elements are painted."
)]
pub enum Order {
  #[enum_value("Painted behind all floating windows.")]
  Background = 1 << 0,
  #[enum_value("Special layer between panels and windows.")]
  PanelResizeLine = 1 << 1,
  #[enum_value("Normal moveable windows that you reorder by click.")]
  Middle = 1 << 2,
  #[enum_value("Popups, menus etc that should always be painted on top of windows. Foreground objects can also have tooltips.")]
  Foreground = 1 << 3,
  #[enum_value(
    "Things floating on top of everything else, like tooltips. You cannot interact with these."
  )]
  Tooltip = 1 << 4,
  #[enum_value("Debug layer, always painted last / on top.")]
  Debug = 1 << 5,
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

struct TabData {
  title: Option<Var>,
  contents: Option<shards::Shards>,
}

impl TabData {
  fn get_title(&self) -> &str {
    if let Some(title) = &self.title {
      title.try_into().unwrap()
    } else {
      "Untitled".into()
    }
  }
  fn activate(
    &self,
    ctx: &shards::types::Context,
    ui_parents: &mut ParamVar,
    ui: &mut egui::Ui,
  ) -> WireState {
    unsafe {
      if let Some(shards) = self.contents {
        with_object_stack_var(ui_parents, ui, &EGUI_UI_TYPE, || {
          let input = Var::default();
          let mut output = Var::default();
          let _wire_state: WireState = (*shards::core::Core).runShards.unwrap()(
            shards,
            ctx as *const _ as *mut _,
            &input,
            &mut output,
          )
          .into();

          Ok(())
        })
        .unwrap();
      }
      return WireState::Continue;
    }
  }

  fn new(title: &mut ParamVar, contents: &mut ShardsVar) -> Self {
    let t = title.get();
    Self {
      title: if !t.is_none() { Some(t.clone()) } else { None },
      contents: if contents.is_empty() {
        None
      } else {
        let shards: Shards = (&*contents).into();
        Some(shards)
      },
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
  tabs: egui_dock::DockState<TabData>,
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

#[derive(shards::shards_enum)]
#[enum_info(
  b"egWF",
  "WindowFlags",
  "Window flags for customizing window behavior and appearance."
)]
pub enum WindowFlags {
  #[enum_value("Do not display the title bar.")]
  NoTitleBar = 1 << 0,
  #[enum_value("Do not allow resizing the window.")]
  NoResize = 1 << 1,
  #[enum_value("Display scrollbars.")]
  Scrollbars = 1 << 2,
  #[enum_value("Do not display the collapse button.")]
  NoCollapse = 1 << 3,
  #[enum_value("Do not allow window movement.")]
  Immovable = 1 << 4,
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
  register_enum::<WindowFlags>();
  register_legacy_shard::<BottomPanel>();
  register_legacy_shard::<CentralPanel>();
  register_legacy_shard::<LeftPanel>();
  register_legacy_shard::<RightPanel>();
  register_legacy_shard::<TopPanel>();
  register_shard::<overlay::OverlayShard>();
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
