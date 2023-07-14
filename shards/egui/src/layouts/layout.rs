/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::Layout;
use super::LayoutAlign;
use super::LayoutDirection;
use crate::layouts::LAYOUT_ALIGN_TYPES;
use crate::layouts::LAYOUT_DIRECTION_TYPES;
use crate::util;
use crate::FLOAT2_VAR_SLICE;
use crate::HELP_OUTPUT_EQUAL_INPUT;
use crate::PARENTS_UI_NAME;
use shards::shard::Shard;
use shards::types::common_type;
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
use shards::types::STRING_OR_NONE_SLICE;

lazy_static! {
  static ref LAYOUT_PARAMETERS: Parameters = vec![
    (
      cstr!("Contents"),
      shccstr!("The UI contents."),
      &SHARDS_OR_NONE_TYPES[..],
    )
      .into(),
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
      shccstr!("Whether to justify the cross axis. Whether widgets get maximum width/height for vertical/horizontal layouts."),
      FLOAT2_VAR_SLICE,
    )
      .into(),
  ];
}

impl Default for Layout {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
      contents: ShardsVar::default(),
      main_dir: ParamVar::default(),
      main_wrap: ParamVar::default(),
      main_align: ParamVar::default(),
      main_justify: ParamVar::default(),
      cross_align: ParamVar::default(),
      cross_justify: ParamVar::default(),
      size: ParamVar::default(),
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
      1 => Ok(self.main_dir.set_param(value)),
      2 => Ok(self.main_wrap.set_param(value)),
      3 => Ok(self.main_align.set_param(value)),
      4 => Ok(self.main_justify.set_param(value)),
      5 => Ok(self.cross_align.set_param(value)),
      6 => Ok(self.cross_justify.set_param(value)),
      7 => Ok(self.size.set_param(value)),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.contents.get_param(),
      1 => self.main_dir.get_param(),
      2 => self.main_wrap.get_param(),
      3 => self.main_align.get_param(),
      4 => self.main_justify.get_param(),
      5 => self.cross_align.get_param(),
      6 => self.cross_justify.get_param(),
      7 => self.size.get_param(),
      _ => Var::default(),
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
    self.requiring.clear();

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
    self.main_dir.warmup(ctx);
    self.main_wrap.warmup(ctx);
    self.main_align.warmup(ctx);
    self.main_justify.warmup(ctx);
    self.cross_align.warmup(ctx);
    self.cross_justify.warmup(ctx);
    self.size.warmup(ctx);

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.size.cleanup();
    self.cross_justify.cleanup();
    self.cross_align.cleanup();
    self.main_justify.cleanup();
    self.main_align.cleanup();
    self.main_wrap.cleanup();
    self.main_dir.cleanup();
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
      let main_dir = self.main_dir.get();

      if main_dir.is_none() {
        return Err("No main direction specified");
      }

      let cross_align = if self.cross_align.get().is_none() {
        egui::Align::Min // default cross align
      } else {
        let cross_align = self.cross_align.get();
        if cross_align.valueType == crate::shardsc::SHType_Enum {
          let bits = unsafe { cross_align.payload.__bindgen_anon_1.__bindgen_anon_3.enumValue };
          match (LayoutAlign { bits }) {
            LayoutAlign::Min => egui::Align::Min,
            LayoutAlign::Left => egui::Align::Min,
            LayoutAlign::Top => egui::Align::Min,
            LayoutAlign::Center => egui::Align::Center,
            LayoutAlign::Max => egui::Align::Max,
            LayoutAlign::Right => egui::Align::Max,
            LayoutAlign::Bottom => egui::Align::Max,
            _ => return Err("Invalid cross alignment provided"),
          }
        } else {
          return Err("Invalid cross alignment type");
        }
      };

      let mut layout = if main_dir.valueType == crate::shardsc::SHType_Enum {
        let bits = unsafe { main_dir.payload.__bindgen_anon_1.__bindgen_anon_3.enumValue };
        match (LayoutDirection { bits }) {
          LayoutDirection::LeftToRight => {
            egui::Layout::from_main_dir_and_cross_align(egui::Direction::LeftToRight, cross_align)
          }
          LayoutDirection::RightToLeft => {
            egui::Layout::from_main_dir_and_cross_align(egui::Direction::RightToLeft, cross_align)
          }
          LayoutDirection::TopDown => {
            egui::Layout::from_main_dir_and_cross_align(egui::Direction::TopDown, cross_align)
          }
          LayoutDirection::BottomUp => {
            egui::Layout::from_main_dir_and_cross_align(egui::Direction::BottomUp, cross_align)
          }
          _ => return Err("Invalid main direction provided"),
        }
      } else {
        return Err("Invalid main direction type");
      };

      let main_align = if self.main_align.get().is_none() {
        egui::Align::Center // default main align
      } else {
        let main_align = self.main_align.get();
        if main_align.valueType == crate::shardsc::SHType_Enum {
          let bits = unsafe { main_align.payload.__bindgen_anon_1.__bindgen_anon_3.enumValue };
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
          return Err("Invalid main alignment type");
        }
      };
      layout = layout.with_main_align(main_align);

      let main_wrap = if self.main_wrap.get().is_none() {
        false // default main wrap
      } else {
        self.main_wrap.get().try_into()?
      };
      layout = layout.with_main_wrap(main_wrap);

      let main_justify = if self.main_justify.get().is_none() {
        false // default main justify
      } else {
        self.main_justify.get().try_into()?
      };
      layout = layout.with_main_justify(main_justify);

      let cross_justify = if self.cross_justify.get().is_none() {
        false // default cross justify
      } else {
        self.cross_justify.get().try_into()?
      };
      layout = layout.with_cross_justify(cross_justify);

      let size = if self.size.get().is_none() {
        (
          ui.available_size_before_wrap().x,
          ui.available_size_before_wrap().y,
        ) // if none then take up all available space
      } else {
        let mut size: (f32, f32) = self.size.get().try_into()?;
        if size.0 == 0.0 {
          size.0 = ui.available_size_before_wrap().x;
        }
        if size.1 == 0.0 {
          size.1 = ui.available_size_before_wrap().y;
        }
        size
      };

      ui.allocate_ui_with_layout(size.into(), layout, |ui| {
        util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.contents)
      })
      .inner?;

      // Always passthrough the input
      Ok(*input)
    } else {
      Err("No UI parent")
    }
  }
}
