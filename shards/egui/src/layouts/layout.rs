/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::Layout;
use super::LayoutAlign;
use super::LayoutAlignCC;
use super::LayoutClass;
use super::LayoutConstructor;
use super::LayoutDirection;
use super::LayoutDirectionCC;
use super::LayoutFrame;
use super::LayoutFrameCC;
use super::ScrollVisibility;
use crate::LAYOUT_FRAME_TYPE_VEC;
use crate::layouts::ScrollVisibilityCC;
use crate::util;
use crate::EguiId;
use crate::ANY_TABLE_SLICE;
use crate::EGUI_UI_TYPE;
use crate::FLOAT2_VAR_SLICE;
use crate::HELP_OUTPUT_EQUAL_INPUT;
use crate::LAYOUTCLASS_TYPE;
use crate::LAYOUTCLASS_TYPE_VEC;
use crate::LAYOUTCLASS_TYPE_VEC_VAR;
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
use std::rc::Rc;

macro_rules! retrieve_parameter {
  ($table:ident, $name:literal, $type:ty) => {
    if let Some(value) = $table.get_static($name) {
      let value: $type = value.try_into().map_err(|e| {
        shlog!("{}: {}", $name, e);
        "Invalid attribute value received"
      })?;
      Some(value)
    } else {
      None
    }
  };
  ($table:ident, $name:literal, $type:ty, $convert:expr) => {
    if let Some(value) = $table.get_static($name) {
      let value: $type = value.try_into().map_err(|e| {
        shlog!("{}: {}", $name, e);
        "Invalid attribute value received"
      })?;
      Some($convert(value)?)
    } else {
      None
    }
  };
}

macro_rules! retrieve_enum_parameter {
  ($table:ident, $name:literal, $type:ident, $typeId:expr) => {
    if let Some(value) = $table.get_static($name) {
      if value.valueType == crate::shardsc::SHType_Enum
        && unsafe { value.payload.__bindgen_anon_1.__bindgen_anon_3.enumTypeId == $typeId }
      {
        // TODO: can double check that this is correct
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
      None
    }
  };
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
      cstr!("Layout"),
      shccstr!("The parameters relating to the layout of the UI element."),
      ANY_TABLE_SLICE,
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
      cstr!("Disabled"),
      shccstr!("Whether the drawn layout should be disabled or not."),
      BOOL_VAR_OR_NONE_SLICE,
    )
      .into(),
    (
      cstr!("Frame"),
      shccstr!("The frame to be drawn around the layout."),
      &LAYOUT_FRAME_TYPE_VEC[..],
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
      &LAYOUTCLASS_TYPE_VEC_VAR[..],
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
    Self {
      parent: ParamVar::default(),
      layout: ParamVar::default(),
      layout_class: None,
      size: ParamVar::default(),
      fill_width: ParamVar::default(),
      fill_height: ParamVar::default(),
      disabled: ParamVar::default(),
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
    &LAYOUTCLASS_TYPE_VEC
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&LAYOUT_CONSTRUCTOR_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => Ok(self.parent.set_param(value)),
      1 => Ok(self.layout.set_param(value)),
      2 => Ok(self.size.set_param(value)),
      3 => Ok(self.fill_width.set_param(value)),
      4 => Ok(self.fill_height.set_param(value)),
      5 => Ok(self.disabled.set_param(value)),
      6 => Ok(self.frame.set_param(value)),
      7 => Ok(self.scroll_area.set_param(value)),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.parent.get_param(),
      1 => self.layout.get_param(),
      2 => self.size.get_param(),
      3 => self.fill_width.get_param(),
      4 => self.fill_height.get_param(),
      5 => self.disabled.get_param(),
      6 => self.frame.get_param(),
      7 => self.scroll_area.get_param(),
      _ => Var::default(),
    }
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.parent.warmup(ctx);
    self.layout.warmup(ctx);
    self.size.warmup(ctx);
    self.fill_width.warmup(ctx);
    self.fill_height.warmup(ctx);
    self.disabled.warmup(ctx);
    self.frame.warmup(ctx);
    self.scroll_area.warmup(ctx);

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.scroll_area.cleanup();
    self.frame.cleanup();
    self.disabled.cleanup();
    self.fill_height.cleanup();
    self.fill_width.cleanup();
    self.size.cleanup();
    self.layout.cleanup();
    self.parent.cleanup();

    Ok(())
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    let parent_layout_class = if !self.parent.get().is_none() {
      let parent_layout_class: Option<Rc<LayoutClass>> = Some(Var::from_object_as_clone(
        self.parent.get(),
        &LAYOUTCLASS_TYPE,
      )?);
      let parent_layout_class = unsafe {
        let parent_layout_ptr = Rc::as_ptr(parent_layout_class.as_ref().unwrap()) as *mut LayoutClass;
        &*parent_layout_ptr
      };  

      Some(parent_layout_class)
    } else {
      None
    };

    let layout = if !self.layout.get().is_none() {
      let layout_table = self.layout.get();
      if layout_table.valueType == crate::shardsc::SHType_Table {
        let layout_table: Table = layout_table.try_into()?;

        let cross_align = if let Some(cross_align) =
          retrieve_enum_parameter!(layout_table, "cross_align", LayoutAlign, LayoutAlignCC)
        {
          cross_align
        } else {
          LayoutAlign::Min // default cross align
        };
        let cross_align: egui::Align = cross_align.try_into()?;

        let main_direction = if let Some(main_direction) = retrieve_enum_parameter!(
          layout_table,
          "main_direction",
          LayoutDirection,
          LayoutDirectionCC
        ) {
          main_direction
        } else {
          return Err("Invalid main direction provided. Main direction is a required parameter");
        };
        let main_direction: egui::Direction = main_direction.try_into()?;

        let mut layout = egui::Layout::from_main_dir_and_cross_align(main_direction, cross_align);

        let main_align = if let Some(main_align) =
          retrieve_enum_parameter!(layout_table, "main_align", LayoutAlign, LayoutAlignCC)
        {
          main_align
        } else {
          LayoutAlign::Center // default main align
        };
        let main_align: egui::Align = main_align.try_into()?;
        layout = layout.with_main_align(main_align);

        let main_wrap =
          if let Some(main_wrap) = retrieve_parameter!(layout_table, "main_wrap", bool) {
            main_wrap
          } else {
            false // default main wrap
          };
        layout = layout.with_main_wrap(main_wrap);

        let main_justify =
          if let Some(main_justify) = retrieve_parameter!(layout_table, "main_wrap", bool) {
            main_justify
          } else {
            false // default main justify
          };
        layout = layout.with_main_justify(main_justify);

        let cross_justify =
          if let Some(cross_justify) = retrieve_parameter!(layout_table, "main_wrap", bool) {
            cross_justify
          } else {
            false // default cross justify
          };
        layout = layout.with_cross_justify(cross_justify);

        layout
      } else {
        return Err("Invalid attribute value received. Expected Table for Layout");
      }
    } else {
      if let Some(parent_layout_class) = parent_layout_class {
        parent_layout_class.layout
      } else {
        return Err("Invalid Layout provided. Layout is a required parameter when there is no parent LayoutClass provided");
      }
    };

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

    let disabled = if !self.disabled.get().is_none() {
      self.disabled.get().try_into()?
    } else {
      if let Some(parent_layout_class) = parent_layout_class {
        parent_layout_class.disabled
      } else {
        false // default disabled value
      }
    };

    let frame = if !self.frame.get().is_none() {
      let frame = self.frame.get();
      if frame.valueType == crate::shardsc::SHType_Enum && unsafe { frame.payload.__bindgen_anon_1.__bindgen_anon_3.enumTypeId == LayoutFrameCC } {
        let bits = unsafe { frame.payload.__bindgen_anon_1.__bindgen_anon_3.enumValue };
        Some(LayoutFrame { bits })
      } else {
        // should be unreachable due to parameter type checking
        return Err("Invalid frame type provided. Expected LayoutFrame for Frame")
      }
    } else {
      if let Some(parent_layout_class) = parent_layout_class {
        parent_layout_class.frame
      } else {
        None
      }
    };

    let scroll_area = if !self.scroll_area.get().is_none() {
      let scroll_area = self.scroll_area.get();
      if scroll_area.valueType == crate::shardsc::SHType_Table {
        let scroll_area: Table = scroll_area.try_into()?;

        let horizontal_scroll_enabled = if let Some(enabled) =
          retrieve_parameter!(scroll_area, "horizontal_scroll_enabled", bool)
        {
          enabled
        } else {
          false
        };

        let vertical_scroll_enabled = if let Some(enabled) =
          retrieve_parameter!(scroll_area, "vertical_scroll_enabled", bool)
        {
          enabled
        } else {
          false
        };

        let scroll_visibility = if let Some(visibility) = retrieve_enum_parameter!(
          scroll_area,
          "scroll_visibility",
          ScrollVisibility,
          ScrollVisibilityCC
        ) {
          visibility
        } else {
          ScrollVisibility::AlwaysVisible
        };
        let scroll_visibility: egui::scroll_area::ScrollBarVisibility =
          scroll_visibility.try_into()?;

        Some(
          egui::ScrollArea::new([horizontal_scroll_enabled, vertical_scroll_enabled])
            .scroll_bar_visibility(scroll_visibility),
        )
      } else {
        return Err("Invalid scroll bar type provided. Expected Table for ScrollArea");
      }
    } else {
      if let Some(parent_layout_class) = parent_layout_class {
        parent_layout_class.scroll_area.clone()
      } else {
        None
      }
    };

    self.layout_class = Some(Rc::new(LayoutClass {
      layout,
      size,
      fill_width,
      fill_height,
      disabled,
      frame,
      scroll_area,
    }));

    let layout_class_ref = self.layout_class.as_ref().unwrap();
    Ok(Var::new_object(layout_class_ref, &LAYOUTCLASS_TYPE))
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
      *LAYOUTCLASS_TYPE,
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
      let layout_class: Option<Rc<LayoutClass>> = Some(Var::from_object_as_clone(
        self.layout_class.get(),
        &LAYOUTCLASS_TYPE,
      )?);
      let layout_class = unsafe {
        let layout_ptr = Rc::as_ptr(layout_class.as_ref().unwrap()) as *mut LayoutClass;
        &*layout_ptr
      };

      let mut layout = layout_class.layout;

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

      let disabled = layout_class.disabled;

      let frame = if let Some(frame) = layout_class.frame {
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
            _ => unreachable!()
        }
      } else {
        None
      };

      let scroll_area = if let Some(scroll_area) = &layout_class.scroll_area {
        Some(scroll_area.clone())
      } else {
        None
      };

      let scroll_area_id = EguiId::new(self, 0); // TODO: Check if have scroll area first

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
