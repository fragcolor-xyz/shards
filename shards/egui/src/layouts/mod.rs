/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use shards::core::register_enum;
use shards::core::register_legacy_shard;
use shards::types::common_type;
use shards::types::ClonedVar;
use shards::types::ExposedTypes;
use shards::types::ParamVar;
use shards::types::ShardsVar;
use shards::types::Type;

struct CollapsingHeader {
  parents: ParamVar,
  requiring: ExposedTypes,
  text: ParamVar,
  header: ShardsVar,
  contents: ShardsVar,
  default_open: ParamVar,
  exposing: ExposedTypes,
}

struct Columns {
  parents: ParamVar,
  requiring: ExposedTypes,
  contents: ParamVar,
  shards: Vec<ShardsVar>,
  exposing: ExposedTypes,
}

struct Disable {
  parents: ParamVar,
  requiring: ExposedTypes,
  contents: ShardsVar,
  disable: ParamVar,
  exposing: ExposedTypes,
}

struct Frame {
  parents: ParamVar,
  requiring: ExposedTypes,
  innerMargin: ParamVar,
  outerMargin: ParamVar,
  rounding: ParamVar,
  fillColor: ParamVar,
  strokeColor: ParamVar,
  strokeWidth: ParamVar,
  contents: ShardsVar,
  exposing: ExposedTypes,
}

struct Grid {
  parents: ParamVar,
  requiring: ExposedTypes,
  contents: ShardsVar,
  striped: ParamVar,
  num_columns: ParamVar,
  min_width: ParamVar,
  max_width: ParamVar,
  spacing: ParamVar,
  exposing: ExposedTypes,
}

struct Group {
  parents: ParamVar,
  requiring: ExposedTypes,
  contents: ShardsVar,
  exposing: ExposedTypes,
}

struct Horizontal {
  parents: ParamVar,
  requiring: ExposedTypes,
  contents: ShardsVar,
  wrap: bool,
  centered: bool,
  exposing: ExposedTypes,
}

#[derive(Clone)]
struct EguiScrollAreaSettings {
  enable_horizontal_scroll_bar: bool,
  enable_vertical_scroll_bar: bool,
  min_width: f32,
  min_height: f32,
  max_width: f32,
  max_height: f32,
  auto_shrink_width: bool,
  auto_shrink_height: bool,
  scroll_visibility: ScrollVisibility,
  enable_scrolling: bool,
}

impl EguiScrollAreaSettings {
  pub fn to_egui_scrollarea(&self) -> egui::ScrollArea {
    let scroll_visibility = self.scroll_visibility.clone();
    egui::ScrollArea::new([
      self.enable_horizontal_scroll_bar,
      self.enable_vertical_scroll_bar,
    ])
    .scroll_bar_visibility(scroll_visibility.into())
    .max_width(self.max_width)
    .max_height(self.max_height)
    .min_scrolled_width(self.min_width)
    .min_scrolled_height(self.min_height)
    .auto_shrink([self.auto_shrink_width, self.auto_shrink_height])
    .enable_scrolling(self.enable_scrolling)
  }
}

#[derive(shards::shards_enum, Clone)]
#[enum_info(b"egSV", "ScrollVisibility", "Scroll bar visibility options")]
pub enum ScrollVisibility {
  #[enum_value("The scroll bars will always be visible.")]
  AlwaysVisible = 1 << 0,
  #[enum_value("The scroll bars will only be visible when needed")]
  VisibleWhenNeeded = 1 << 1,
  #[enum_value("The scroll bars will always be hidden.")]
  AlwaysHidden = 1 << 2,
}

impl From<ScrollVisibility> for egui::scroll_area::ScrollBarVisibility {
  fn from(value: ScrollVisibility) -> Self {
    match value {
      ScrollVisibility::AlwaysVisible => egui::scroll_area::ScrollBarVisibility::AlwaysVisible,
      ScrollVisibility::VisibleWhenNeeded => {
        egui::scroll_area::ScrollBarVisibility::VisibleWhenNeeded
      }
      ScrollVisibility::AlwaysHidden => egui::scroll_area::ScrollBarVisibility::AlwaysHidden,
    }
  }
}

#[derive(shards::shards_enum, Clone)]
#[enum_info(b"egLF", "LayoutFrame", "Frame styling options")]
pub enum LayoutFrame {
  #[enum_value("Use the frame styling for grouping widgets together.")]
  Widgets = 1 << 0,
  #[enum_value("Use the frame styling for a side top panel.")]
  SideTopPanel = 1 << 1,
  #[enum_value("Use the frame styling for the central panel.")]
  CentralPanel = 1 << 2,
  #[enum_value("Use the frame styling for a window.")]
  Window = 1 << 3,
  #[enum_value("Use the frame styling for a menu.")]
  Menu = 1 << 4,
  #[enum_value("Use the frame styling for a popup.")]
  Popup = 1 << 5,
  #[enum_value("Use the frame styling for a canvas to draw on.")]
  Canvas = 1 << 6,
  #[enum_value("Use the frame styling for a dark canvas to draw on.")]
  DarkCanvas = 1 << 7,
}

#[derive(shards::shards_enum, Clone)]
#[enum_info(b"egLD", "LayoutDirection", "Layout direction options")]
pub enum LayoutDirection {
  #[enum_value(
    "Describes a horizontal layout where its contents are arranged from the left to the right."
  )]
  LeftToRight = 1 << 0,
  #[enum_value(
    "Describes a horizontal layout where its contents are arranged from the right to the left."
  )]
  RightToLeft = 1 << 1,
  #[enum_value(
    "Describes a vertical layout where its contents are arranged from the top to the bottom."
  )]
  TopDown = 1 << 2,
  #[enum_value(
    "Describes a vertical layout where its contents are arranged from the bottom to the top."
  )]
  BottomUp = 1 << 3,
}

impl From<LayoutDirection> for egui::Direction {
  fn from(value: LayoutDirection) -> Self {
    match value {
      LayoutDirection::LeftToRight => egui::Direction::LeftToRight,
      LayoutDirection::RightToLeft => egui::Direction::RightToLeft,
      LayoutDirection::TopDown => egui::Direction::TopDown,
      LayoutDirection::BottomUp => egui::Direction::BottomUp,
    }
  }
}

#[derive(shards::shards_enum, Clone)]
#[enum_info(b"egLA", "LayoutAlign", "Layout alignment options")]
pub enum LayoutAlign {
  #[enum_value("Left or top alignment for e.g. anchors and layouts.")]
  Min = 1 << 0,
  #[enum_value("Left alignment for e.g. anchors and layouts.")]
  Left = 1 << 1,
  #[enum_value("Top alignment for e.g. anchors and layouts.")]
  Top = 1 << 2,
  #[enum_value("Horizontal or vertical center alignment for e.g. anchors and layouts.")]
  Center = 1 << 3,
  #[enum_value("Right or bottom center alignment for e.g. anchors and layouts.")]
  Max = 1 << 4,
  #[enum_value("Right alignment for e.g. anchors and layouts.")]
  Right = 1 << 5,
  #[enum_value("Bottom center alignment for e.g. anchors and layouts.")]
  Bottom = 1 << 6,
}

impl From<LayoutAlign> for egui::Align {
  fn from(value: LayoutAlign) -> Self {
    match value {
      LayoutAlign::Min => egui::Align::Min,
      LayoutAlign::Left => egui::Align::Min,
      LayoutAlign::Top => egui::Align::Min,
      LayoutAlign::Center => egui::Align::Center,
      LayoutAlign::Max => egui::Align::Max,
      LayoutAlign::Right => egui::Align::Max,
      LayoutAlign::Bottom => egui::Align::Max,
    }
  }
}

lazy_static! {
  static ref SCROLL_VISIBILITY_OR_NONE_SLICE: Vec<Type> =
    vec![*SCROLLVISIBILITY_TYPE, common_type::none];
  static ref LAYOUT_FRAME_OR_NONE_SLICE: Vec<Type> = vec![*LAYOUTFRAME_TYPE, common_type::none];
  static ref LAYOUT_DIRECTION_OR_NONE_SLICE: Vec<Type> =
    vec![*LAYOUTDIRECTION_TYPE, common_type::none];
  static ref LAYOUT_ALIGN_OR_NONE_SLICE: Vec<Type> = vec![*LAYOUTALIGN_TYPE, common_type::none];
}

struct Indent {
  parents: ParamVar,
  requiring: ExposedTypes,
  contents: ShardsVar,
  exposing: ExposedTypes,
}

struct NextRow {
  parents: ParamVar,
  requiring: ExposedTypes,
}

struct ScrollArea {
  parents: ParamVar,
  requiring: ExposedTypes,
  contents: ShardsVar,
  horizontal: ParamVar,
  vertical: ParamVar,
  alwaysShow: ParamVar,
  exposing: ExposedTypes,
}

struct Separator {
  parents: ParamVar,
  requiring: ExposedTypes,
}

struct Space {
  parents: ParamVar,
  requiring: ExposedTypes,
  amount: ParamVar,
}

struct Table {
  parents: ParamVar,
  requiring: ExposedTypes,
  rows: ClonedVar,
  columns: ClonedVar,
  striped: ParamVar,
  resizable: ParamVar,
  row_index: ParamVar,
  shards: Vec<ShardsVar>,
  header_shards: Vec<Option<ShardsVar>>,
  exposing: ExposedTypes,
}

struct Vertical {
  parents: ParamVar,
  requiring: ExposedTypes,
  contents: ShardsVar,
  centered: bool,
  exposing: ExposedTypes,
}

struct Sized {
  parents: ParamVar,
  contents: ShardsVar,
  requiring: ExposedTypes,
  exposing: ExposedTypes,
  width: ClonedVar,
  height: ClonedVar,
  fill_width: ClonedVar,
  fill_height: ClonedVar,
}

pub(crate) mod auto_grid;
mod collapsing_header;
mod columns;
mod disable;
mod frame;
mod grid;
mod group;
mod horizontal;
mod indent;
mod layout;
mod scroll_area;
mod separator;
mod sized;
mod space;
mod table;
mod vertical;

pub fn register_shards() {
  auto_grid::register_shards();
  layout::register_shards();
  register_legacy_shard::<CollapsingHeader>();
  register_legacy_shard::<Columns>();
  register_legacy_shard::<Disable>();
  register_legacy_shard::<Frame>();
  register_legacy_shard::<Grid>();
  register_legacy_shard::<Group>();
  register_legacy_shard::<Horizontal>();
  register_enum::<ScrollVisibility>();
  register_enum::<LayoutFrame>();
  register_enum::<LayoutDirection>();
  register_enum::<LayoutAlign>();
  register_legacy_shard::<Indent>();
  register_legacy_shard::<NextRow>();
  register_legacy_shard::<ScrollArea>();
  register_legacy_shard::<Separator>();
  register_legacy_shard::<Space>();
  register_legacy_shard::<Table>();
  register_legacy_shard::<Vertical>();
  register_legacy_shard::<Sized>();
}
