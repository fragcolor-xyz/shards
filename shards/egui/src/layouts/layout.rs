/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::EguiScrollAreaSettings;
use super::LayoutAlign;
use super::LayoutAlignCC;
use super::LayoutDirection;
use super::LayoutDirectionCC;
use super::LayoutFrame;
use super::LayoutFrameCC;
use super::ScrollVisibility;
use crate::layouts::ScrollVisibilityCC;
use crate::layouts::LAYOUT_ALIGN_TYPES;
use crate::layouts::LAYOUT_DIRECTION_TYPES;
use crate::layouts::SCROLL_VISIBILITY_TYPES;
use crate::util;
use crate::util::with_possible_panic;
use crate::Anchor;
use crate::EguiId;
use crate::ANCHOR_TYPE;
use crate::ANCHOR_TYPES;
use crate::EGUI_UI_TYPE;
use crate::FLOAT2_VAR_SLICE;
use crate::FLOAT_VAR_SLICE;
use crate::HELP_OUTPUT_EQUAL_INPUT;
use crate::LAYOUTCLASS_TYPE;
use crate::LAYOUTCLASS_TYPE_VEC;
use crate::LAYOUTCLASS_TYPE_VEC_VAR;
use crate::LAYOUT_FRAME_TYPE_VEC;
use crate::PARENTS_UI_NAME;
use shards::core::register_legacy_shard;
use shards::core::{register_enum, register_shard};
use shards::shard::LegacyShard;
use shards::shard::Shard;
use shards::types::Context;
use shards::types::ExposedInfo;
use shards::types::ExposedTypes;
use shards::types::InstanceData;
use shards::types::OptionalString;
use shards::types::ParamVar;
use shards::types::Parameters;
use shards::types::ShardsVar;
use shards::types::Type;
use shards::types::Types;
use shards::types::Var;
use shards::types::ANY_TYPES;
use shards::types::BOOL_TYPES;
use shards::types::BOOL_VAR_OR_NONE_SLICE;
use shards::types::SHARDS_OR_NONE_TYPES;
use shards::types::{common_type, NONE_TYPES};
use std::rc::Rc;

struct LayoutClass {
  parent: *const LayoutClass,
  layout: Option<egui::Layout>,
  min_size: Option<(f32, f32)>,
  max_size: Option<(f32, f32)>,
  fill_width: Option<bool>,
  fill_height: Option<bool>,
  disabled: Option<bool>,
  frame: Option<LayoutFrame>,
  scroll_area: Option<EguiScrollAreaSettings>,
}

struct LayoutConstructor {
  parent: ParamVar,
  layout_class: Option<Rc<LayoutClass>>,
  main_direction: ParamVar,
  main_wrap: ParamVar,
  main_align: ParamVar,
  main_justify: ParamVar,
  cross_align: ParamVar,
  cross_justify: ParamVar,
  min_size: ParamVar,
  max_size: ParamVar,
  fill_width: ParamVar,
  fill_height: ParamVar,
  disabled: ParamVar,
  frame: ParamVar,
  enable_horizontal_scroll_bar: ParamVar,
  enable_vertical_scroll_bar: ParamVar,
  scroll_visibility: ParamVar,
  scroll_area_min_width: ParamVar,
  scroll_area_min_height: ParamVar,
  scroll_area_max_width: ParamVar,
  scroll_area_max_height: ParamVar,
  scroll_area_auto_shrink_width: ParamVar,
  scroll_area_auto_shrink_height: ParamVar,
  scroll_area_enable_scrolling: ParamVar,
}

macro_rules! retrieve_parameter {
  ($param:ident, $name:literal, $type:ty) => {
    if !$param.is_none() {
      let value: $type = $param.try_into().map_err(|e| {
        shlog!("{}: {}", $name, e);
        "Invalid attribute value received"
      })?;
      Some(value)
    } else {
      None
    }
  };
}

macro_rules! retrieve_enum_parameter {
  ($param:ident, $name:literal, $type:ident, $typeId:expr) => {
    // Check if parameter has been passed in, and if so retrieve it
    if !$param.is_none() {
      let value = $param;
      // Check for correct enum type
      if value.valueType == crate::shardsc::SHType_Enum
        && unsafe { value.payload.__bindgen_anon_1.__bindgen_anon_3.enumTypeId == $typeId }
      {
        // Retrieve value
        let bits = unsafe { value.payload.__bindgen_anon_1.__bindgen_anon_3.enumValue };
        let value = $type { bits };
        Some(value)
      } else {
        Err("Expected Enum value of same Enum type.").map_err(|e| {
          shlog!("{}: {}", $name, e);
          "Invalid attribute value received"
        })?
      }
    } else {
      // No parameter to retrieve from, caller should retrieve from parent or use default value
      None
    }
  };
}

macro_rules! retrieve_layout_class_attribute {
  ($layout_class:ident, $attribute:ident) => {{
    let mut parent = $layout_class as *const LayoutClass;
    let mut result = None;
    while !parent.is_null() {
      unsafe {
        if let Some(attribute) = &(*parent).$attribute {
          // found the attribute, can return now
          result = Some(attribute.clone());
          break;
        } else {
          // attribute not found in parent, continue looking through parents
          parent = (*parent).parent;
        }
      }
    }
    result
  }};
  ($layout_class:ident, $attribute:ident, $member:ident) => {{
    let mut parent = $layout_class as *const LayoutClass;
    let mut result = None;
    while !parent.is_null() {
      unsafe {
        if let Some(attribute) = &(*parent).$attribute {
          // found the member attribute, can return now
          result = Some(attribute.$member.clone());
          break;
        } else {
          // attribute not found in parent, continue looking through parents
          parent = (*parent).parent;
        }
      }
    }
    result
  }};
}

lazy_static! {
  static ref LAYOUT_CONSTRUCTOR_PARAMETERS: Parameters = vec![
    (
      cstr!("Parent"),
      shccstr!("The parent Layout class to inherit parameters from."),
      &LAYOUTCLASS_TYPE_VEC_VAR[..],
    )
      .into(),
    (
      cstr!("MainDirection"),
      shccstr!("The primary direction of the UI element layout."),
      &LAYOUT_DIRECTION_TYPES[..],
    )
      .into(),
    (
      cstr!("MainWrap"),
      shccstr!("Should UI elements wrap when reaching the end of the main direction."),
      BOOL_VAR_OR_NONE_SLICE,
    )
      .into(),
    (
      cstr!("MainAlign"),
      shccstr!("Alignment of UI elements along the main axis."),
      &LAYOUT_ALIGN_TYPES[..],
    )
      .into(),
    (
      cstr!("MainJustify"),
      shccstr!("Justification of UI elements along the main axis."),
      BOOL_VAR_OR_NONE_SLICE,
    )
      .into(),
    (
      cstr!("CrossAlign"),
      shccstr!("Alignment of UI elements along the cross axis."),
      &LAYOUT_ALIGN_TYPES[..],
    )
      .into(),
    (
      cstr!("CrossJustify"),
      shccstr!("Justification of UI elements along the cross axis."),
      BOOL_VAR_OR_NONE_SLICE,
    )
      .into(),
    (
      cstr!("MinSize"),
      shccstr!("Minimum space reserved for UI contents. Overridden by FillWidth and FillHeight."),
      FLOAT2_VAR_SLICE,
    )
      .into(),
    (
      cstr!("MaxSize"),
      shccstr!("Maximum space reserved for UI contents. Overridden by FillWidth and FillHeight."),
      FLOAT2_VAR_SLICE,
    )
      .into(),
    (
      cstr!("FillWidth"),
      shccstr!("Whether the layout should occupy the full width."),
      &BOOL_TYPES[..],
    )
      .into(),
    (
      cstr!("FillHeight"),
      shccstr!("Whether the layout should occupy the full height."),
      &BOOL_TYPES[..],
    )
      .into(),
    (
      cstr!("Disabled"),
      shccstr!("Whether the layout should be disabled."),
      BOOL_VAR_OR_NONE_SLICE,
    )
      .into(),
    (
      cstr!("Frame"),
      shccstr!("Frame to be drawn around the layout."),
      &LAYOUT_FRAME_TYPE_VEC[..],
    )
      .into(),
    (
      cstr!("EnableHorizontalScrollBar"),
      shccstr!("Enable the horizontal scroll bar. Creates a ScrollArea if true."),
      BOOL_VAR_OR_NONE_SLICE,
    )
      .into(),
    (
      cstr!("EnableVerticalScrollBar"),
      shccstr!("Enable the vertical scroll bar. Creates a ScrollArea if true."),
      BOOL_VAR_OR_NONE_SLICE,
    )
      .into(),
    (
      cstr!("ScrollBarVisibility"),
      shccstr!("Visibility of the scroll bars: AlwaysVisible, VisibleWhenNeeded, or AlwaysHidden. Default: AlwaysVisible."),
      &SCROLL_VISIBILITY_TYPES[..],
    )
      .into(),
    (
      cstr!("ScrollAreaMinWidth"),
      shccstr!("Minimum width of the scroll area."),
      FLOAT_VAR_SLICE,
    )
      .into(),
    (
      cstr!("ScrollAreaMinHeight"),
      shccstr!("Minimum height of the scroll area."),
      FLOAT_VAR_SLICE,
    )
      .into(),
    (
      cstr!("ScrollAreaMaxWidth"),
      shccstr!("Maximum width of the scroll area."),
      FLOAT_VAR_SLICE,
    )
      .into(),
    (
      cstr!("ScrollAreaMaxHeight"),
      shccstr!("Maximum height of the scroll area."),
      FLOAT_VAR_SLICE,
    )
      .into(),
    (
      cstr!("ScrollAreaAutoShrinkWidth"),
      shccstr!("Auto-shrink scroll area width to fit contents."),
      BOOL_VAR_OR_NONE_SLICE,
    )
      .into(),
    (
      cstr!("ScrollAreaAutoShrinkHeight"),
      shccstr!("Auto-shrink scroll area height to fit contents."),
      BOOL_VAR_OR_NONE_SLICE,
    )
      .into(),
    (
      cstr!("ScrollAreaEnableScrolling"),
      shccstr!("Enable scrolling in the scroll area."),
      BOOL_VAR_OR_NONE_SLICE,
    )
      .into(),
  ];

  static ref LAYOUT_PARAMETERS: Parameters = vec![
    (
      cstr!("Contents"),
      shccstr!("The UI contents."),
      &SHARDS_OR_NONE_TYPES[..],
    )
      .into(),
    (
      cstr!("Class"),
      shccstr!("Layout class describing all layout options."),
      &LAYOUTCLASS_TYPE_VEC_VAR[..],
    )
      .into(),
    (
      cstr!("MinSize"),
      shccstr!("Minimum space reserved for UI contents. Overridden by FillWidth and FillHeight."),
      FLOAT2_VAR_SLICE,
    )
      .into(),
    (
      cstr!("MaxSize"),
      shccstr!("Maximum space reserved for UI contents. Overridden by FillWidth and FillHeight."),
      FLOAT2_VAR_SLICE,
    )
      .into(),
    (
      cstr!("FillWidth"),
      shccstr!("Whether the layout should occupy the full width."),
      &BOOL_TYPES[..],
    )
      .into(),
    (
      cstr!("FillHeight"),
      shccstr!("Whether the layout should occupy the full height."),
      &BOOL_TYPES[..],
    )
      .into(),
  ];
}

impl Default for LayoutConstructor {
  fn default() -> Self {
    Self {
      parent: ParamVar::default(),
      layout_class: None,
      main_direction: ParamVar::default(),
      main_wrap: ParamVar::default(),
      main_align: ParamVar::default(),
      main_justify: ParamVar::default(),
      cross_align: ParamVar::default(),
      cross_justify: ParamVar::default(),
      min_size: ParamVar::default(),
      max_size: ParamVar::default(),
      fill_width: ParamVar::default(),
      fill_height: ParamVar::default(),
      disabled: ParamVar::default(),
      frame: ParamVar::default(),
      enable_horizontal_scroll_bar: ParamVar::default(),
      enable_vertical_scroll_bar: ParamVar::default(),
      scroll_visibility: ParamVar::default(),
      scroll_area_min_width: ParamVar::default(),
      scroll_area_min_height: ParamVar::default(),
      scroll_area_max_width: ParamVar::default(),
      scroll_area_max_height: ParamVar::default(),
      scroll_area_auto_shrink_width: ParamVar::default(),
      scroll_area_auto_shrink_height: ParamVar::default(),
      scroll_area_enable_scrolling: ParamVar::default(),
    }
  }
}

impl LegacyShard for LayoutConstructor {
  fn registerName() -> &'static str {
    cstr!("UI.LayoutClass")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("UI.LayoutClass-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.LayoutClass"
  }

  fn inputHelp(&mut self) -> OptionalString {
    "Not used.".into()
  }

  fn outputHelp(&mut self) -> OptionalString {
    "A Layout class that can be used in other UI shards.".into()
  }

  fn help(&mut self) -> OptionalString {
    "This shard creates a Layout class that can be used in other UI shards.".into()
  }

  fn inputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn outputTypes(&mut self) -> &Types {
    &LAYOUTCLASS_TYPE_VEC
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&LAYOUT_CONSTRUCTOR_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &'static str> {
    match index {
      0 => self.parent.set_param(value),
      1 => self.main_direction.set_param(value),
      2 => self.main_wrap.set_param(value),
      3 => self.main_align.set_param(value),
      4 => self.main_justify.set_param(value),
      5 => self.cross_align.set_param(value),
      6 => self.cross_justify.set_param(value),
      7 => self.min_size.set_param(value),
      8 => self.max_size.set_param(value),
      9 => self.fill_width.set_param(value),
      10 => self.fill_height.set_param(value),
      11 => self.disabled.set_param(value),
      12 => self.frame.set_param(value),
      13 => self.enable_horizontal_scroll_bar.set_param(value),
      14 => self.enable_vertical_scroll_bar.set_param(value),
      15 => self.scroll_visibility.set_param(value),
      16 => self.scroll_area_min_width.set_param(value),
      17 => self.scroll_area_min_height.set_param(value),
      18 => self.scroll_area_max_width.set_param(value),
      19 => self.scroll_area_max_height.set_param(value),
      20 => self.scroll_area_auto_shrink_width.set_param(value),
      21 => self.scroll_area_auto_shrink_height.set_param(value),
      22 => self.scroll_area_enable_scrolling.set_param(value),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.parent.get_param(),
      1 => self.main_direction.get_param(),
      2 => self.main_wrap.get_param(),
      3 => self.main_align.get_param(),
      4 => self.main_justify.get_param(),
      5 => self.cross_align.get_param(),
      6 => self.cross_justify.get_param(),
      7 => self.min_size.get_param(),
      8 => self.max_size.get_param(),
      9 => self.fill_width.get_param(),
      10 => self.fill_height.get_param(),
      11 => self.disabled.get_param(),
      12 => self.frame.get_param(),
      13 => self.enable_horizontal_scroll_bar.get_param(),
      14 => self.enable_vertical_scroll_bar.get_param(),
      15 => self.scroll_visibility.get_param(),
      16 => self.scroll_area_min_width.get_param(),
      17 => self.scroll_area_min_height.get_param(),
      18 => self.scroll_area_max_width.get_param(),
      19 => self.scroll_area_max_height.get_param(),
      20 => self.scroll_area_auto_shrink_width.get_param(),
      21 => self.scroll_area_auto_shrink_height.get_param(),
      22 => self.scroll_area_enable_scrolling.get_param(),
      _ => Var::default(),
    }
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.parent.warmup(ctx);
    self.main_direction.warmup(ctx);
    self.main_wrap.warmup(ctx);
    self.main_align.warmup(ctx);
    self.main_justify.warmup(ctx);
    self.cross_align.warmup(ctx);
    self.cross_justify.warmup(ctx);
    self.min_size.warmup(ctx);
    self.max_size.warmup(ctx);
    self.fill_width.warmup(ctx);
    self.fill_height.warmup(ctx);
    self.disabled.warmup(ctx);
    self.frame.warmup(ctx);
    self.enable_horizontal_scroll_bar.warmup(ctx);
    self.enable_vertical_scroll_bar.warmup(ctx);
    self.scroll_visibility.warmup(ctx);
    self.scroll_area_min_width.warmup(ctx);
    self.scroll_area_min_height.warmup(ctx);
    self.scroll_area_max_width.warmup(ctx);
    self.scroll_area_max_height.warmup(ctx);
    self.scroll_area_auto_shrink_width.warmup(ctx);
    self.scroll_area_auto_shrink_height.warmup(ctx);
    self.scroll_area_enable_scrolling.warmup(ctx);

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.scroll_area_enable_scrolling.cleanup(ctx);
    self.scroll_area_auto_shrink_height.cleanup(ctx);
    self.scroll_area_auto_shrink_width.cleanup(ctx);
    self.scroll_area_max_height.cleanup(ctx);
    self.scroll_area_max_width.cleanup(ctx);
    self.scroll_area_min_height.cleanup(ctx);
    self.scroll_area_min_width.cleanup(ctx);
    self.scroll_visibility.cleanup(ctx);
    self.enable_vertical_scroll_bar.cleanup(ctx);
    self.enable_horizontal_scroll_bar.cleanup(ctx);
    self.frame.cleanup(ctx);
    self.disabled.cleanup(ctx);
    self.fill_height.cleanup(ctx);
    self.fill_width.cleanup(ctx);
    self.max_size.cleanup(ctx);
    self.min_size.cleanup(ctx);
    self.cross_justify.cleanup(ctx);
    self.cross_align.cleanup(ctx);
    self.main_justify.cleanup(ctx);
    self.main_align.cleanup(ctx);
    self.main_wrap.cleanup(ctx);
    self.main_direction.cleanup(ctx);
    self.parent.cleanup(ctx);

    Ok(())
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    let mut has_parent = false;

    let parent_layout_class = if !self.parent.get().is_none() {
      let parent_layout_class: Option<Rc<LayoutClass>> = Some(Var::from_object_as_clone(
        self.parent.get(),
        &LAYOUTCLASS_TYPE,
      )?);
      let parent_layout_class = unsafe {
        let parent_layout_ptr =
          Rc::as_ptr(parent_layout_class.as_ref().unwrap()) as *mut LayoutClass;
        &*parent_layout_ptr
      };

      has_parent = true;
      Some(parent_layout_class)
    } else {
      None
    };

    // Track if a new layout has to be created. This is in the case that the layout class is new or there has been an overwrite in its values
    let mut create_new_layout = false;

    let cross_align = self.cross_align.get();
    let cross_align = if let Some(cross_align) =
      retrieve_enum_parameter!(cross_align, "cross_align", LayoutAlign, LayoutAlignCC)
    {
      create_new_layout = true;
      cross_align.into()
    } else {
      // if there is a parent, retrieve the parent's value over the default
      if let Some(parent_layout_class) = parent_layout_class {
        // this is guaranteed to be Some because the parent has already been constructed
        retrieve_layout_class_attribute!(parent_layout_class, layout, cross_align).unwrap()
      } else {
        create_new_layout = true;
        egui::Align::Min // default cross aligns
      }
    };

    let main_direction = self.main_direction.get();
    let main_direction = if let Some(main_direction) = retrieve_enum_parameter!(
      main_direction,
      "main_direction",
      LayoutDirection,
      LayoutDirectionCC
    ) {
      create_new_layout = true;
      main_direction.into()
    } else {
      if let Some(parent_layout_class) = parent_layout_class {
        retrieve_layout_class_attribute!(parent_layout_class, layout, main_dir).unwrap()
      } else {
        return Err("Invalid main direction provided. Main direction is a required parameter");
      }
    };

    let main_align = self.main_align.get();
    let main_align = if let Some(main_align) =
      retrieve_enum_parameter!(main_align, "main_align", LayoutAlign, LayoutAlignCC)
    {
      create_new_layout = true;
      main_align.into()
    } else {
      if let Some(parent_layout_class) = parent_layout_class {
        retrieve_layout_class_attribute!(parent_layout_class, layout, main_align).unwrap()
      } else {
        create_new_layout = true;
        egui::Align::Center // default main align
      }
    };

    let main_wrap = self.main_wrap.get();
    let main_wrap = if let Some(main_wrap) = retrieve_parameter!(main_wrap, "main_wrap", bool) {
      create_new_layout = true;
      main_wrap
    } else {
      if let Some(parent_layout_class) = parent_layout_class {
        retrieve_layout_class_attribute!(parent_layout_class, layout, main_wrap).unwrap()
      } else {
        create_new_layout = true;
        false // default main wrap
      }
    };

    let main_justify = self.main_justify.get();
    let main_justify =
      if let Some(main_justify) = retrieve_parameter!(main_justify, "main_justify", bool) {
        create_new_layout = true;
        main_justify
      } else {
        if let Some(parent_layout_class) = parent_layout_class {
          retrieve_layout_class_attribute!(parent_layout_class, layout, main_justify).unwrap()
        } else {
          create_new_layout = true;
          false // default main justify
        }
      };

    let cross_justify = self.cross_justify.get();
    let cross_justify =
      if let Some(cross_justify) = retrieve_parameter!(cross_justify, "cross_justify", bool) {
        create_new_layout = true;
        cross_justify
      } else {
        if let Some(parent_layout_class) = parent_layout_class {
          retrieve_layout_class_attribute!(parent_layout_class, layout, cross_justify).unwrap()
        } else {
          create_new_layout = true;
          false // default cross justify
        }
      };

    let mut layout = if create_new_layout {
      Some(
        egui::Layout::from_main_dir_and_cross_align(main_direction, cross_align)
          .with_main_align(main_align)
          .with_main_wrap(main_wrap)
          .with_main_justify(main_justify)
          .with_cross_justify(cross_justify),
      )
    } else {
      if has_parent {
        None
      } else {
        return Err("Invalid Layout provided. Layout is a required parameter when there is no parent LayoutClass provided");
      }
    };

    let min_size = if !self.min_size.get().is_none() {
      Some(self.min_size.get().try_into()?)
    } else {
      None
    };

    let max_size = if !self.max_size.get().is_none() {
      Some(self.max_size.get().try_into()?)
    } else {
      None
    };

    let fill_width: Option<bool> = if !self.fill_width.get().is_none() {
      Some(self.fill_width.get().try_into()?)
    } else {
      None // default fill width
    };
    let fill_height: Option<bool> = if !self.fill_height.get().is_none() {
      Some(self.fill_height.get().try_into()?)
    } else {
      None // default fill height
    };

    let disabled = if !self.disabled.get().is_none() {
      Some(self.disabled.get().try_into()?)
    } else {
      None // default value should be interpreted as false later on
    };

    let frame = if !self.frame.get().is_none() {
      let frame = self.frame.get();
      if frame.valueType == crate::shardsc::SHType_Enum
        && unsafe { frame.payload.__bindgen_anon_1.__bindgen_anon_3.enumTypeId == LayoutFrameCC }
      {
        let bits = unsafe { frame.payload.__bindgen_anon_1.__bindgen_anon_3.enumValue };
        Some(LayoutFrame { bits })
      } else {
        // should be unreachable due to parameter type checking
        return Err("Invalid frame type provided. Expected LayoutFrame for Frame");
      }
    } else {
      None
    };

    let enable_horizontal_scroll_bar = self.enable_horizontal_scroll_bar.get();
    let enable_horizontal_scroll_bar = if let Some(enable_horizontal_scroll_bar) = retrieve_parameter!(
      enable_horizontal_scroll_bar,
      "enable_horizontal_scroll_bar",
      bool
    ) {
      enable_horizontal_scroll_bar
    } else {
      if let Some(parent_layout_class) = parent_layout_class {
        retrieve_layout_class_attribute!(
          parent_layout_class,
          scroll_area,
          enable_horizontal_scroll_bar
        )
        .unwrap_or_default()
      } else {
        false // default enable_horizontal_scroll_bar
      }
    };

    let enable_vertical_scroll_bar = self.enable_vertical_scroll_bar.get();
    let enable_vertical_scroll_bar = if let Some(enable_vertical_scroll_bar) = retrieve_parameter!(
      enable_vertical_scroll_bar,
      "enable_vertical_scroll_bar",
      bool
    ) {
      enable_vertical_scroll_bar
    } else {
      if let Some(parent_layout_class) = parent_layout_class {
        retrieve_layout_class_attribute!(
          parent_layout_class,
          scroll_area,
          enable_vertical_scroll_bar
        )
        .unwrap_or_default()
      } else {
        false // default enable_vertical_scroll_bar
      }
    };

    let create_new_scroll_area = enable_horizontal_scroll_bar || enable_vertical_scroll_bar;

    let scroll_visibility = self.scroll_visibility.get();
    let scroll_visibility = if let Some(scroll_visibility) = retrieve_enum_parameter!(
      scroll_visibility,
      "scroll_visibility",
      ScrollVisibility,
      ScrollVisibilityCC
    ) {
      scroll_visibility
    } else {
      if let Some(parent_layout_class) = parent_layout_class {
        retrieve_layout_class_attribute!(parent_layout_class, scroll_area, scroll_visibility)
          .unwrap_or(ScrollVisibility::AlwaysVisible)
      } else {
        ScrollVisibility::AlwaysVisible // Default should normally be VisibleWhenNeeded, but it is buggy at the moment
      }
    };

    const MIN_SCROLLING_SIZE: f32 = 64.0; // default value for min_scrolling_width and min_scrolling_height as of egui 0.22.0

    let min_width = self.scroll_area_min_width.get();
    let min_width =
      if let Some(min_width) = retrieve_parameter!(min_width, "scroll_area_min_width", f32) {
        min_width
      } else {
        if let Some(parent_layout_class) = parent_layout_class {
          retrieve_layout_class_attribute!(parent_layout_class, scroll_area, min_width)
            .unwrap_or(MIN_SCROLLING_SIZE)
        } else {
          MIN_SCROLLING_SIZE // default min_width
        }
      };

    let min_height = self.scroll_area_min_height.get();
    let min_height =
      if let Some(min_height) = retrieve_parameter!(min_height, "scroll_area_min_height", f32) {
        min_height
      } else {
        if let Some(parent_layout_class) = parent_layout_class {
          retrieve_layout_class_attribute!(parent_layout_class, scroll_area, min_height)
            .unwrap_or(MIN_SCROLLING_SIZE)
        } else {
          MIN_SCROLLING_SIZE // default min_height
        }
      };

    let max_width = self.scroll_area_max_width.get();
    let max_width =
      if let Some(max_width) = retrieve_parameter!(max_width, "scroll_area_max_width", f32) {
        max_width
      } else {
        if let Some(parent_layout_class) = parent_layout_class {
          retrieve_layout_class_attribute!(parent_layout_class, scroll_area, max_width)
            .unwrap_or(f32::INFINITY)
        } else {
          f32::INFINITY // default max_width
        }
      };

    let max_height = self.scroll_area_max_height.get();
    let max_height =
      if let Some(max_height) = retrieve_parameter!(max_height, "scroll_area_max_height", f32) {
        max_height
      } else {
        if let Some(parent_layout_class) = parent_layout_class {
          retrieve_layout_class_attribute!(parent_layout_class, scroll_area, max_height)
            .unwrap_or(f32::INFINITY)
        } else {
          f32::INFINITY // default max_height
        }
      };

    let auto_shrink_width = self.scroll_area_auto_shrink_width.get();
    let auto_shrink_width = if let Some(auto_shrink_width) =
      retrieve_parameter!(auto_shrink_width, "scroll_area_auto_shrink_width", bool)
    {
      auto_shrink_width
    } else {
      if let Some(parent_layout_class) = parent_layout_class {
        retrieve_layout_class_attribute!(parent_layout_class, scroll_area, auto_shrink_width)
          .unwrap_or(true)
      } else {
        true // default auto_shrink_width
      }
    };

    let auto_shrink_height = self.scroll_area_auto_shrink_height.get();
    let auto_shrink_height = if let Some(auto_shrink_height) =
      retrieve_parameter!(auto_shrink_height, "scroll_area_auto_shrink_height", bool)
    {
      auto_shrink_height
    } else {
      if let Some(parent_layout_class) = parent_layout_class {
        retrieve_layout_class_attribute!(parent_layout_class, scroll_area, auto_shrink_height)
          .unwrap_or(true)
      } else {
        true // default auto_shrink_height
      }
    };

    let enable_scrolling = self.scroll_area_enable_scrolling.get();
    let enable_scrolling = if let Some(enable_scrolling) =
      retrieve_parameter!(enable_scrolling, "scroll_area_enable_scrolling", bool)
    {
      enable_scrolling
    } else {
      if let Some(parent_layout_class) = parent_layout_class {
        retrieve_layout_class_attribute!(parent_layout_class, scroll_area, enable_scrolling)
          .unwrap_or(true)
      } else {
        true // default enable_scrolling
      }
    };

    let mut scroll_area = if create_new_scroll_area {
      Some(EguiScrollAreaSettings {
        enable_horizontal_scroll_bar,
        enable_vertical_scroll_bar,
        min_width,
        min_height,
        max_width,
        max_height,
        auto_shrink_width,
        auto_shrink_height,
        scroll_visibility,
        enable_scrolling,
      })
    } else {
      // Whether there is a parent or not, if there is no scroll area override, then there is no need to create a new scroll area object
      None
    };

    if let Some(parent_layout_class) = parent_layout_class {
      self.layout_class = Some(Rc::new(LayoutClass {
        parent: parent_layout_class,
        layout,
        min_size,
        max_size,
        fill_width,
        fill_height,
        disabled,
        frame,
        scroll_area,
      }));
    } else {
      self.layout_class = Some(Rc::new(LayoutClass {
        parent: std::ptr::null(),
        layout,
        min_size,
        max_size,
        fill_width,
        fill_height,
        disabled,
        frame,
        scroll_area,
      }));
    }

    let layout_class_ref = self.layout_class.as_ref().unwrap();
    Ok(Var::new_object(layout_class_ref, &LAYOUTCLASS_TYPE))
  }
}

lazy_static! {
  static ref ANCHOR_OR_NONE_TYPES: Vec<Type> = {
    let mut v = ANCHOR_TYPES.to_vec();
    v.push(common_type::none);
    v
  };
  static ref POSITION_TYPES: Vec<Type> = vec![
    common_type::float2,
    common_type::float2_var,
    common_type::none
  ];
}

#[derive(shards::shard)]
#[shard_info("UI.Layout", "Versatile layout with numerous customization options.")]
struct LayoutShard {
  #[shard_required]
  requiring: ExposedTypes,
  inner_exposed: ExposedTypes,
  #[shard_warmup]
  parents: ParamVar,
  #[shard_param("Contents", "The UI contents.", SHARDS_OR_NONE_TYPES)]
  contents: ShardsVar,
  #[shard_param(
    "Class",
    "The Layout class defining all layout options.",
    LAYOUTCLASS_TYPE_VEC_VAR
  )]
  layout_class: ParamVar,
  #[shard_param(
    "MinSize",
    "Minimum reserved space for the UI. Overridden by FillWidth and FillHeight.",
    FLOAT2_VAR_SLICE
  )]
  min_size: ParamVar,
  #[shard_param(
    "MaxSize",
    "Maximum reserved space for the UI. Overridden by FillWidth and FillHeight.",
    FLOAT2_VAR_SLICE
  )]
  max_size: ParamVar,
  #[shard_param(
    "FillWidth",
    "Whether the layout should occupy the full width.",
    BOOL_TYPES
  )]
  fill_width: ParamVar,
  #[shard_param(
    "FillHeight",
    "Whether the layout should occupy the full height.",
    BOOL_TYPES
  )]
  fill_height: ParamVar,
}

impl Default for LayoutShard {
  fn default() -> Self {
    Self {
      requiring: ExposedTypes::new(),
      inner_exposed: ExposedTypes::new(),
      parents: ParamVar::new_named(PARENTS_UI_NAME),
      contents: ShardsVar::default(),
      layout_class: ParamVar::default(),
      min_size: ParamVar::default(),
      max_size: ParamVar::default(),
      fill_width: ParamVar::default(),
      fill_height: ParamVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for LayoutShard {
  fn input_types(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn input_help(&mut self) -> OptionalString {
    "Not used.".into()
  }

  fn output_types(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn output_help(&mut self) -> OptionalString {
    "Passthrough the input.".into()
  }

  fn exposed_variables(&mut self) -> Option<&ExposedTypes> {
    Some(&self.inner_exposed)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;
    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;

    if !self.layout_class.is_variable() {
      return Err("Class parameter is required");
    }

    self.inner_exposed.clear();
    if !self.contents.is_empty() {
      let composed = self.contents.compose(data)?;
      shards::util::merge_exposed_types(&mut self.inner_exposed, &composed.exposedInfo);
      shards::util::merge_exposed_types(&mut self.requiring, &composed.requiredInfo);
    }
    shards::util::expose_shards_contents(&mut self.inner_exposed, &self.contents);
    shards::util::require_shards_contents(&mut self.requiring, &self.contents);

    util::require_parents(&mut self.requiring);

    // Always passthrough the input
    Ok(data.inputType)
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    let ui = util::get_parent_ui(self.parents.get())?;
    let layout_class: Option<Rc<LayoutClass>> = Some(Var::from_object_as_clone(
      self.layout_class.get(),
      &LAYOUTCLASS_TYPE,
    )?);
    let layout_class = unsafe {
      let layout_ptr = Rc::as_ptr(layout_class.as_ref().unwrap()) as *mut LayoutClass;
      &*layout_ptr
    };

    let layout = if let Some(layout) = retrieve_layout_class_attribute!(layout_class, layout) {
      layout
    } else {
      return Err("No layout found in LayoutClass. LayoutClass is invalid/corrupted");
    };

    let mut min_size = if !self.min_size.get().is_none() {
      self.min_size.get().try_into()?
    } else {
      if let Some(min_size) = retrieve_layout_class_attribute!(layout_class, min_size) {
        min_size
      } else {
        (0.0, 0.0) // default value for min_size
      }
    };

    let mut max_size = if !self.max_size.get().is_none() {
      Some(self.max_size.get().try_into()?)
    } else {
      if let Some(max_size) = retrieve_layout_class_attribute!(layout_class, max_size) {
        Some(max_size)
      } else {
        None // default value for max_size (no max size)
      }
    };

    // shard parameters have higher priority and override layout class
    let fill_width = if !self.fill_width.get().is_none() {
      self.fill_width.get().try_into()?
    } else {
      if let Some(fill_width) = retrieve_layout_class_attribute!(layout_class, fill_width) {
        fill_width
      } else {
        false // default value for fill_width
      }
    };

    let fill_height = if !self.fill_height.get().is_none() {
      self.fill_height.get().try_into()?
    } else {
      if let Some(fill_height) = retrieve_layout_class_attribute!(layout_class, fill_height) {
        fill_height
      } else {
        false // default value for fill_height
      }
    };

    let mut min_size: egui::Vec2 = min_size.into();

    let mut max_size = if let Some(max_size) = max_size {
      egui::Vec2::new(max_size.0, max_size.1)
    } else {
      ui.available_size_before_wrap() // try to take up all available space (no max)
    };

    if fill_width {
      min_size.x = ui.available_size_before_wrap().x;
      max_size.x = ui.available_size_before_wrap().x;
    }
    if fill_height {
      min_size.y = ui.available_size_before_wrap().y;
      max_size.y = ui.available_size_before_wrap().y;
    }

    // If the size is still 0, use only minimum size for an interactive widget
    if min_size.x == 0.0 {
      min_size.x = ui.spacing().interact_size.x;
    }
    if min_size.y == 0.0 {
      min_size.y = ui.spacing().interact_size.y;
    }

    let disabled = if let Some(disabled) = retrieve_layout_class_attribute!(layout_class, disabled)
    {
      disabled
    } else {
      false // default value for disabled
    };

    let frame = if let Some(frame) = retrieve_layout_class_attribute!(layout_class, frame) {
      let style = ui.style();
      match frame {
        LayoutFrame::Widgets => Some(egui::Frame::group(style)),
        LayoutFrame::SideTopPanel => Some(egui::Frame::side_top_panel(style)),
        LayoutFrame::CentralPanel => Some(egui::Frame::central_panel(style)),
        LayoutFrame::Window => Some(egui::Frame::window(style)),
        LayoutFrame::Menu => Some(egui::Frame::menu(style)),
        LayoutFrame::Popup => Some(egui::Frame::popup(style)),
        LayoutFrame::Canvas => Some(egui::Frame::canvas(style)),
        LayoutFrame::DarkCanvas => Some(egui::Frame::dark_canvas(style)),
        _ => unreachable!(),
      }
    } else {
      None // default value for frame
    };

    let scroll_area =
      if let Some(scroll_area) = retrieve_layout_class_attribute!(layout_class, scroll_area) {
        Some(scroll_area.to_egui_scrollarea())
      } else {
        None // default value for scroll_area
      };

    let scroll_area_id = EguiId::new(self, 0); // TODO: Check if have scroll area first

    with_possible_panic(|| {
      // if there is a frame, draw it as the outermost ui element
      if let Some(frame) = frame {
        frame
          .show(ui, |ui| {
            // set whether all widgets in the contents are enabled or disabled
            ui.set_enabled(!disabled);
            // add the new child_ui created by frame onto the stack of parents
            // inside of frame
            // render scroll area and inner layout if there is a scroll area
            if let Some(scroll_area) = scroll_area {
              scroll_area
                .id_source(scroll_area_id)
                .show(ui, |ui| {
                  // inside of scroll area
                  ui.allocate_ui_with_layout(max_size, layout, |ui| {
                    ui.set_min_size(min_size); // set minimum size of entire layout

                    if self.contents.is_empty() {
                      return Ok(*input);
                    }
                    util::activate_ui_contents(
                      context,
                      input,
                      ui,
                      &mut self.parents,
                      &mut self.contents,
                    )
                  })
                  .inner
                })
                .inner
            } else {
              // inside of frame, no scroll area to render, render inner layout
              ui.allocate_ui_with_layout(max_size, layout, |ui| {
                ui.set_min_size(min_size); // set minimum size of entire layout

                if self.contents.is_empty() {
                  return Ok(*input);
                }

                util::activate_ui_contents(
                  context,
                  input,
                  ui,
                  &mut self.parents,
                  &mut self.contents,
                )
              })
              .inner
            }
          })
          .inner
      } else {
        // no frame to render, render only the scroll area (if applicable) and inner layout
        if let Some(scroll_area) = scroll_area {
          scroll_area
            .id_source(scroll_area_id)
            .show(ui, |ui| {
              // inside of scroll area
              ui.allocate_ui_with_layout(max_size, layout, |ui| {
                ui.set_min_size(min_size); // set minimum size of entire layout

                if self.contents.is_empty() {
                  return Ok(*input);
                }

                util::activate_ui_contents(
                  context,
                  input,
                  ui,
                  &mut self.parents,
                  &mut self.contents,
                )
              })
              .inner
            })
            .inner
        } else {
          // inside of frame, no scroll area to render, render inner layout
          ui.allocate_ui_with_layout(max_size, layout, |ui| {
            ui.set_min_size(min_size); // set minimum size of entire layout

            if self.contents.is_empty() {
              return Ok(*input);
            }

            util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.contents)
          })
          .inner
        }
      }
    })??;

    // Always passthrough the input
    Ok(*input)
  }
}

pub fn register_shards() {
  register_legacy_shard::<LayoutConstructor>();
  register_shard::<LayoutShard>();
}
