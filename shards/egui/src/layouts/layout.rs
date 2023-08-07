/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::Layout;
use super::LayoutAlign;
use super::LayoutClass;
use super::LayoutConstructor;
use super::LayoutDirection;
use super::ScrollVisibility;
use crate::layouts::LAYOUT_ALIGN_TYPES;
use crate::layouts::LAYOUT_DIRECTION_TYPES;
use crate::misc::style_util;
use crate::util;
use crate::EguiId;
use crate::ANY_TABLE_SLICE;
use crate::EGUI_UI_TYPE;
use crate::FLOAT2_VAR_SLICE;
use crate::HELP_OUTPUT_EQUAL_INPUT;
use crate::LAYOUT_TYPE;
use crate::LAYOUT_TYPE_VEC;
use crate::LAYOUT_TYPE_VEC_VAR;
use crate::LAYOUT_VAR_TYPE;
use crate::PARENTS_UI_NAME;
use shards::shard::Shard;
use shards::types::Context;
use shards::types::ExposedInfo;
use shards::types::ExposedTypes;
use shards::types::InstanceData;
use shards::types::OptionalString;
use shards::types::ParamVar;
use shards::types::Parameters;
use shards::types::ShardsVar;
use shards::types::Table;
use shards::types::Type;
use shards::types::Types;
use shards::types::Var;
use shards::types::ANY_TYPES;
use shards::types::BOOL_TYPES;
use shards::types::BOOL_VAR_OR_NONE_SLICE;
use shards::types::SHARDS_OR_NONE_TYPES;
use shards::types::STRING_OR_NONE_SLICE;
use shards::SHColor;
use std::rc::Rc;

macro_rules! retrieve_parameter {
  ($table:ident, $name:literal, $type:ty, $default:expr) => {
    if let Some(value) = $table.get_static($name) {
      let value: $type = value.try_into().map_err(|e| {
        shlog!("{}: {}", $name, e);
        "Invalid attribute value received"
      })?;
      value
    } else {
      $default
    }
  };
  ($table:ident, $name:literal, $type:ty, $convert:expr, $default:expr) => {
    if let Some(value) = $table.get_static($name) {
      let value: $type = value.try_into().map_err(|e| {
        shlog!("{}: {}", $name, e);
        "Invalid attribute value received"
      })?;
      $convert(value)?
    } else {
      $default
    }
  };
}

lazy_static! {
  static ref LAYOUT_CONSTRUCTOR_PARAMETERS: Parameters = vec![
    (
      cstr!("Direction"),
      shccstr!("The main axis direction. LeftToRight, RightToLeft, TopDown, or BottomUp."),
      &LAYOUT_DIRECTION_TYPES[..],
    )
      .into(),
    (
      cstr!("MainWrap"),
      shccstr!("If true, wrap around when reaching the end of the main direction."),
      BOOL_VAR_OR_NONE_SLICE,
    )
      .into(),
    (
      cstr!("MainAlign"),
      shccstr!("The alignment of content on the main axis. Min, Center or Max."),
      &LAYOUT_ALIGN_TYPES[..],
    )
      .into(),
    (
      cstr!("MainJustify"),
      shccstr!("Whether to justify the main axis."),
      BOOL_VAR_OR_NONE_SLICE,
    )
      .into(),
    (
      cstr!("CrossAlign"),
      shccstr!("The alignment of content on the cross axis. Min, Center or Max."),
      &LAYOUT_ALIGN_TYPES[..],
    )
      .into(),
    (
      cstr!("CrossJustify"),
      shccstr!("Whether to justify the cross axis. Whether widgets get maximum width/height for vertical/horizontal layouts."),
      BOOL_VAR_OR_NONE_SLICE,
    )
      .into(),
    (
      cstr!("Size"),
      shccstr!("The size of the space to be reserved by this UI. May be overidden by FillWidth and FillHeight."),
      FLOAT2_VAR_SLICE,
    )
      .into(),
    (cstr!("FillWidth"), shccstr!("Whether the Layout should take up the full width of the available space."), &BOOL_TYPES[..],).into(),
    (cstr!("FillHeight"), shccstr!("Whether the Layout should take up the full height of the available space."), &BOOL_TYPES[..],).into(),
    (
      cstr!("Frame"),
      shccstr!("The frame to be drawn around the layout."),
      // &ANY_TABLE_VAR_TYPES
      ANY_TABLE_SLICE,
    )
      .into(),
    (
      cstr!("ScrollArea"),
      shccstr!("The scroll area to be drawn around the layout to provide scroll bars."),
      ANY_TABLE_SLICE,
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
      shccstr!("The Layout class describing all of the options relating to the layout of this UI."),
      &LAYOUT_TYPE_VEC_VAR[..],
    )
    .into(),
    (
      cstr!("Size"),
      shccstr!("The size of the space to be reserved by this UI. May be overidden by FillWidth and FillHeight."),
      FLOAT2_VAR_SLICE,
    )
      .into(),
    (cstr!("FillWidth"), shccstr!("Whether the Layout should take up the full width of the available space."), &BOOL_TYPES[..],).into(),
    (cstr!("FillHeight"), shccstr!("Whether the Layout should take up the full height of the available space."), &BOOL_TYPES[..],).into(),
  ];
}

impl Default for LayoutConstructor {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
      layout: None,
      main_dir: ParamVar::default(),
      main_wrap: ParamVar::default(),
      main_align: ParamVar::default(),
      main_justify: ParamVar::default(),
      cross_align: ParamVar::default(),
      cross_justify: ParamVar::default(),
      size: ParamVar::default(),
      fill_width: ParamVar::default(),
      fill_height: ParamVar::default(),
      frame: ParamVar::default(),
      scroll_area: ParamVar::default(),
    }
  }
}

impl Shard for LayoutConstructor {
  fn registerName() -> &'static str {
    cstr!("UI.LayoutClass")
  }

  fn hash() -> u32 {
    compile_time_crc32::crc32!("UI.LayoutClass-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.LayoutClass"
  }

  fn inputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn outputTypes(&mut self) -> &Types {
    &LAYOUT_TYPE_VEC
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&LAYOUT_CONSTRUCTOR_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => Ok(self.main_dir.set_param(value)),
      1 => Ok(self.main_wrap.set_param(value)),
      2 => Ok(self.main_align.set_param(value)),
      3 => Ok(self.main_justify.set_param(value)),
      4 => Ok(self.cross_align.set_param(value)),
      5 => Ok(self.cross_justify.set_param(value)),
      6 => Ok(self.size.set_param(value)),
      7 => Ok(self.fill_width.set_param(value)),
      8 => Ok(self.fill_height.set_param(value)),
      9 => Ok(self.frame.set_param(value)),
      10 => Ok(self.scroll_area.set_param(value)),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.main_dir.get_param(),
      1 => self.main_wrap.get_param(),
      2 => self.main_align.get_param(),
      3 => self.main_justify.get_param(),
      4 => self.cross_align.get_param(),
      5 => self.cross_justify.get_param(),
      6 => self.size.get_param(),
      7 => self.fill_width.get_param(),
      8 => self.fill_height.get_param(),
      9 => self.frame.get_param(),
      10 => self.scroll_area.get_param(),
      _ => Var::default(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    // Add UI.Parents to the list of required variables
    util::require_parents(&mut self.requiring, &self.parents);

    Some(&self.requiring)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.parents.warmup(ctx);
    self.main_dir.warmup(ctx);
    self.main_wrap.warmup(ctx);
    self.main_align.warmup(ctx);
    self.main_justify.warmup(ctx);
    self.cross_align.warmup(ctx);
    self.cross_justify.warmup(ctx);
    self.size.warmup(ctx);
    self.fill_width.warmup(ctx);
    self.fill_height.warmup(ctx);
    self.frame.warmup(ctx);
    self.scroll_area.warmup(ctx);

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.scroll_area.cleanup();
    self.frame.cleanup();
    self.fill_height.cleanup();
    self.fill_width.cleanup();
    self.size.cleanup();
    self.cross_justify.cleanup();
    self.cross_align.cleanup();
    self.main_justify.cleanup();
    self.main_align.cleanup();
    self.main_wrap.cleanup();
    self.main_dir.cleanup();
    self.parents.cleanup();

    Ok(())
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    let cross_align = if self.cross_align.get().is_none() {
      egui::Align::Min // default cross align
    } else {
      let cross_align = self.cross_align.get();
      if cross_align.valueType == crate::shardsc::SHType_Enum {
        let bits = unsafe {
          cross_align
            .payload
            .__bindgen_anon_1
            .__bindgen_anon_3
            .enumValue
        };
        match (LayoutAlign { bits }) {
          LayoutAlign::Min => egui::Align::Min,
          LayoutAlign::Left => egui::Align::Min,
          LayoutAlign::Top => egui::Align::Min,
          LayoutAlign::Center => egui::Align::Center,
          LayoutAlign::Max => egui::Align::Max,
          LayoutAlign::Right => egui::Align::Max,
          LayoutAlign::Bottom => egui::Align::Max,
          _ => return Err("Invalid cross alignment"),
        }
      } else {
        return Err("Invalid cross alignment type");
      }
    };

    let main_dir = if !self.main_dir.get().is_none() {
      let main_dir = self.main_dir.get();
      if main_dir.valueType == crate::shardsc::SHType_Enum {
        let bits = unsafe { main_dir.payload.__bindgen_anon_1.__bindgen_anon_3.enumValue };
        match (LayoutDirection { bits }) {
          LayoutDirection::LeftToRight => egui::Direction::LeftToRight,

          LayoutDirection::RightToLeft => egui::Direction::RightToLeft,

          LayoutDirection::TopDown => egui::Direction::TopDown,

          LayoutDirection::BottomUp => egui::Direction::BottomUp,

          _ => return Err("Invalid main direction provided"),
        }
      } else {
        return Err("Invalid main direction type");
      }
    } else {
      egui::Direction::LeftToRight // default main direction
    };

    let mut layout = egui::Layout::from_main_dir_and_cross_align(main_dir, cross_align);

    let main_align = if self.main_align.get().is_none() {
      egui::Align::Center // default main align
    } else {
      let main_align = self.main_align.get();
      if main_align.valueType == crate::shardsc::SHType_Enum {
        let bits = unsafe {
          main_align
            .payload
            .__bindgen_anon_1
            .__bindgen_anon_3
            .enumValue
        };
        match (LayoutAlign { bits }) {
          LayoutAlign::Min => egui::Align::Min,
          LayoutAlign::Left => egui::Align::Min,
          LayoutAlign::Top => egui::Align::Min,
          LayoutAlign::Center => egui::Align::Center,
          LayoutAlign::Max => egui::Align::Max,
          LayoutAlign::Right => egui::Align::Max,
          LayoutAlign::Bottom => egui::Align::Max,
          _ => return Err("Invalid main alignment provided"),
        }
      } else {
        return Err("Invalid value for main alignment");
      }
    };
    layout = layout.with_main_align(main_align);

    let main_wrap = if !self.main_wrap.get().is_none() {
      self.main_wrap.get().try_into()?
    } else {
      false // default main wrap
    };
    layout = layout.with_main_wrap(main_wrap);

    let main_justify = if !self.main_justify.get().is_none() {
      self.main_justify.get().try_into()?
    } else {
      false // default main justify
    };
    layout = layout.with_main_justify(main_justify);

    let cross_justify = if !self.cross_justify.get().is_none() {
      self.cross_justify.get().try_into()?
    } else {
      false // default cross justify
    };
    layout = layout.with_cross_justify(cross_justify);

    let size = if !self.size.get().is_none() {
      self.size.get().try_into()?
    } else {
      (0.0, 0.0)
    };

    let fill_width: bool = if !self.fill_width.get().is_none() {
      self.fill_width.get().try_into()?
    } else {
      false // default fill width
    };
    let fill_height: bool = if !self.fill_height.get().is_none() {
      self.fill_height.get().try_into()?
    } else {
      false // default fill height
    };

    let frame = if !self.frame.get().is_none() {
      let frame = self.frame.get();
      if frame.valueType == crate::shardsc::SHType_Table {
        let frame: Table = frame.try_into()?;

        // uses frame defaults for group as per frame::group
        let inner_margin = retrieve_parameter!(
          frame,
          "inner_margin",
          (f32, f32, f32, f32),
          style_util::get_margin,
          egui::style::Margin::same(6.0)
        );

        let outer_margin = retrieve_parameter!(
          frame,
          "outer_margin",
          (f32, f32, f32, f32),
          style_util::get_margin,
          egui::style::Margin::default()
        );

        let rounding = retrieve_parameter!(
          frame,
          "rounding",
          (f32, f32, f32, f32),
          style_util::get_rounding,
          egui::Rounding::same(2.0) // TODO: default value for noninteractive widgets. these values are for light mode, and may not be very good
                                    // ui.style().visuals.widgets.noninteractive.rounding
        );

        let stroke = retrieve_parameter!(
          frame,
          "stroke",
          Table,
          style_util::get_stroke,
          egui::Stroke::new(1.0, egui::Color32::from_gray(190)) // TODO: default value for noninteractive widgets
                                                                // ui.style().visuals.widgets.noninteractive.bg_stroke
        );

        let fill = retrieve_parameter!(
          frame,
          "fill",
          SHColor,
          style_util::get_color,
          egui::Color32::from_gray(248) // TODO: default value for noninteractive widgets
                                        // ui.style().visuals.widgets.noninteractive.bg_fill // TODO: maybe default
        );

        let shadow = retrieve_parameter!(
          frame,
          "shadow",
          Table,
          style_util::get_shadow,
          egui::epaint::Shadow::default()
        );

        Some(egui::Frame {
          inner_margin,
          outer_margin,
          rounding,
          shadow,
          fill,
          stroke,
        })
      } else {
        return Err("Invalid frame type provided");
      }
    } else {
      None
    };

    let scroll_area = if !self.scroll_area.get().is_none() {
      let scroll_area = self.scroll_area.get();
      if scroll_area.valueType == crate::shardsc::SHType_Table {
        let scroll_area: Table = scroll_area.try_into()?;

        let horizontal_scroll_enabled =
          retrieve_parameter!(scroll_area, "horizontal_scroll_enabled", bool, false);

        let vertical_scroll_enabled =
          retrieve_parameter!(scroll_area, "vertical_scroll_enabled", bool, false);

        let scroll_visibility = if let Some(value) = scroll_area.get_static("scroll_visibility") {
          if value.valueType == crate::shardsc::SHType_Enum {
            let bits = unsafe { value.payload.__bindgen_anon_1.__bindgen_anon_3.enumValue };
            match (ScrollVisibility { bits }) {
              ScrollVisibility::AlwaysVisible => {
                egui::scroll_area::ScrollBarVisibility::AlwaysVisible
              }
              ScrollVisibility::VisibleWhenNeeded => {
                egui::scroll_area::ScrollBarVisibility::VisibleWhenNeeded
              }
              ScrollVisibility::AlwaysHidden => {
                egui::scroll_area::ScrollBarVisibility::AlwaysHidden
              }
              _ => return Err("Invalid scroll visibility provided"),
            }
          } else {
            return Err("Invalid type for scroll visibility");
          }
        } else {
          egui::scroll_area::ScrollBarVisibility::AlwaysVisible
        };

        Some(
          egui::ScrollArea::new([horizontal_scroll_enabled, vertical_scroll_enabled])
            .scroll_bar_visibility(scroll_visibility),
        )
      } else {
        return Err("Invalid scroll bar type provided");
      }
    } else {
      None
    };

    self.layout = Some(Rc::new(LayoutClass {
      main_dir,
      main_wrap,
      main_align,
      main_justify,
      cross_align,
      cross_justify,
      size,
      fill_width,
      fill_height,
      frame,
      scroll_area,
    }));

    let layout_ref = self.layout.as_ref().unwrap();
    Ok(Var::new_object(layout_ref, &LAYOUT_TYPE))
  }
}

impl Default for Layout {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
      contents: ShardsVar::default(),
      layout_class: ParamVar::default(),
      size: ParamVar::default(),
      fill_width: ParamVar::default(),
      fill_height: ParamVar::default(),
      exposing: Vec::new(),
    }
  }
}

impl Shard for Layout {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.Layout")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.Layout-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.Layout"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "Versatile layout with many options for customizing the desired UI."
    ))
  }

  fn inputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!(
      "The value that will be passed to the Contents shards of the layout."
    ))
  }

  fn outputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    *HELP_OUTPUT_EQUAL_INPUT
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&LAYOUT_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => self.contents.set_param(value),
      1 => Ok(self.layout_class.set_param(value)),
      2 => Ok(self.size.set_param(value)),
      3 => Ok(self.fill_width.set_param(value)),
      4 => Ok(self.fill_height.set_param(value)),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.contents.get_param(),
      1 => self.layout_class.get_param(),
      2 => self.size.get_param(),
      3 => self.fill_width.get_param(),
      4 => self.fill_height.get_param(),
      _ => Var::default(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

    self.requiring.push(ExposedInfo::new_with_help_from_ptr(
      self.layout_class.get_name(),
      shccstr!("The required layout class."),
      *LAYOUT_TYPE,
    ));

    // Add UI.Parents to the list of required variables
    util::require_parents(&mut self.requiring, &self.parents);

    Some(&self.requiring)
  }

  fn exposedVariables(&mut self) -> Option<&ExposedTypes> {
    self.exposing.clear();

    if util::expose_contents_variables(&mut self.exposing, &self.contents) {
      Some(&self.exposing)
    } else {
      None
    }
  }

  fn hasCompose() -> bool {
    true
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    if !self.contents.is_empty() {
      self.contents.compose(data)?;
    }

    // Always passthrough the input
    Ok(data.inputType)
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.parents.warmup(ctx);
    if !self.contents.is_empty() {
      self.contents.warmup(ctx)?;
    }
    self.layout_class.warmup(ctx);
    self.size.warmup(ctx);
    self.fill_width.warmup(ctx);
    self.fill_height.warmup(ctx);

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.fill_height.cleanup();
    self.fill_width.cleanup();
    self.size.cleanup();
    self.layout_class.cleanup();
    if !self.contents.is_empty() {
      self.contents.cleanup();
    }
    self.parents.cleanup();

    Ok(())
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    if self.contents.is_empty() {
      return Ok(*input);
    }

    if let Some(ui) = util::get_current_parent(self.parents.get())? {
      // if self.layout_class.get().is_none() {
      //   return Err("No class provided.");
      // }

      let layout_class: Option<Rc<LayoutClass>> = Some(Var::from_object_as_clone(
        self.layout_class.get(),
        &LAYOUT_TYPE,
      )?);
      let layout_class = unsafe {
        let layout_ptr = Rc::as_ptr(layout_class.as_ref().unwrap()) as *mut LayoutClass;
        &*layout_ptr
      };

      // let layout_class: Rc<LayoutClass> = Var::from_object_as_clone(self.layout_class.get(), &LAYOUT_VAR_TYPE)?;

      let mut layout = egui::Layout::from_main_dir_and_cross_align(
        layout_class.main_dir,
        layout_class.cross_align,
      );
      layout = layout.with_main_align(layout_class.main_align);
      layout = layout.with_main_wrap(layout_class.main_wrap);
      layout = layout.with_main_justify(layout_class.main_justify);
      layout = layout.with_cross_justify(layout_class.cross_justify);

      let mut size = if !self.size.get().is_none() {
        self.size.get().try_into()?
      } else {
        (0.0, 0.0)
      };

      let fill_width: bool = if !self.fill_width.get().is_none() {
        self.fill_width.get().try_into()?
      } else {
        false // default fill width
      };
      let fill_height: bool = if !self.fill_height.get().is_none() {
        self.fill_height.get().try_into()?
      } else {
        false // default fill height
      };

      if fill_width {
        size.0 = ui.available_size_before_wrap().x;
      }
      if fill_height {
        size.1 = ui.available_size_before_wrap().y;
      }

      // If the size is still 0, use only minimum size for an interactive widget
      if size.0 == 0.0 {
        size.0 = ui.spacing().interact_size.x;
      }
      if size.1 == 0.0 {
        size.1 = ui.spacing().interact_size.y;
      }

      let frame = if let Some(frame) = layout_class.frame {
        Some(frame.clone())
      } else {
        None
      };
      let scroll_area = if let Some(scroll_area) = &layout_class.scroll_area {
        Some(scroll_area.clone())
      } else {
        None
      }; // TODO: Why does scroll area not have copy but frame does?

      let disabled = false;

      // let horizontal_scroll_enabled = true;
      // let vertical_scroll_enabled = true;
      // let has_scroll_area = horizontal_scroll_enabled || vertical_scroll_enabled;
      // let scroll_visibility = true;
      let scroll_area_id = EguiId::new(self, 0); // TODO: Check if have scroll area first

      // let visibility = if scroll_visibility {
      //   egui::scroll_area::ScrollBarVisibility::AlwaysVisible
      // } else {
      //   egui::scroll_area::ScrollBarVisibility::VisibleWhenNeeded
      // };

      // let inner_margin = egui::style::Margin::default();
      // let outer_margin = egui::style::Margin::default();
      // let rounding = ui.style().visuals.widgets.noninteractive.rounding;
      // let stroke = egui::epaint::Stroke {
      //   width: ui.style().visuals.widgets.noninteractive.bg_stroke.width,
      //   color: ui.style().visuals.widgets.noninteractive.bg_stroke.color,
      // };
      // let fill = ui.style().visuals.widgets.noninteractive.bg_fill;

      // egui::ScrollArea::new([
      //   horizontal_scroll_enabled,
      //   vertical_scroll_enabled,
      // ])
      // .id_source(EguiId::new(self, 0))
      // .scroll_area_visibility(visibility)
      // .show(ui, |ui| {
      //   util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.contents)
      // })
      // .inner?;

      if let Some(frame) = frame {
        frame
          .show(ui, |ui| {
            // add the new child_ui created by frame onto the stack of parents
            util::with_object_stack_var_pass_stack_var(
              &mut self.parents,
              ui,
              &EGUI_UI_TYPE,
              |parent_stack_var| {
                if let Some(ui) = util::get_current_parent(parent_stack_var.get())? {
                  ui.allocate_ui_with_layout(size.into(), layout, |ui| {
                    // add the new child_ui created by layout onto the stack of parents
                    if let Some(scroll_area) = scroll_area {
                      util::with_object_stack_var_pass_stack_var(
                        parent_stack_var,
                        ui,
                        &EGUI_UI_TYPE,
                        |parent_stack_var| {
                          if let Some(ui) = util::get_current_parent(parent_stack_var.get())? {
                            // create the scroll area as the last child_ui, then include all the contents
                            scroll_area
                            .id_source(scroll_area_id)
                              .show(ui, |ui| {
                                // set whether all widgets in the contents are enabled or disabled
                                ui.set_enabled(!disabled);
                                util::activate_ui_contents(
                                  context,
                                  input,
                                  ui,
                                  parent_stack_var,
                                  &mut self.contents,
                                )
                              })
                              .inner
                          } else {
                            Err("No UI parent")
                          }
                        },
                      )
                    } else {
                      // else, no need to add child_ui to stack again, we can simply render the contents without scroll area
                      ui.set_enabled(!disabled);
                      util::activate_ui_contents(
                        context,
                        input,
                        ui,
                        parent_stack_var,
                        &mut self.contents,
                      )
                    }
                  })
                  .inner
                } else {
                  Err("No UI parent")
                }
                // let wire_state = contents.activate(context, input, &mut output);
                // if wire_state == WireState::Error {
                //   return Err("Failed to activate contents");
                // }
                // Ok(())
              },
            )

            // Ok(output)
          })
          .inner?;
      } else {
        ui.allocate_ui_with_layout(size.into(), layout, |ui| {
          // add the new child_ui created by layout onto the stack of parents
          if let Some(scroll_area) = scroll_area {
            util::with_object_stack_var_pass_stack_var(
              &mut self.parents,
              ui,
              &EGUI_UI_TYPE,
              |parent_stack_var| {
                if let Some(ui) = util::get_current_parent(parent_stack_var.get())? {
                  // create the scroll area as the last child_ui, then include all the contents
                  scroll_area
                  .id_source(scroll_area_id)
                    .show(ui, |ui| {
                      // set whether all widgets in the contents are enabled or disabled
                      ui.set_enabled(!disabled);
                      util::activate_ui_contents(
                        context,
                        input,
                        ui,
                        parent_stack_var,
                        &mut self.contents,
                      )
                    })
                    .inner
                } else {
                  Err("No UI parent")
                }
              },
            )
          } else {
            // else, no need to add child_ui to stack again, we can simply render the contents without scroll area
            ui.set_enabled(!disabled);
            util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.contents)
          }
        })
        .inner?;
      }

      // ui.allocate_ui_with_layout(size.into(), layout, |ui| {
      //   util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.contents)
      // })
      // .inner?;

      // Always passthrough the input
      Ok(*input)
    } else {
      Err("No UI parent")
    }
  }
}
