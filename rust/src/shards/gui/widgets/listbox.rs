/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::ListBox;
use crate::shard::Shard;
use crate::shards::gui::util;
use crate::shards::gui::INT_VAR_OR_NONE_SLICE;
use crate::shards::gui::PARENTS_UI_NAME;
use crate::shardsc;
use crate::types::common_type;
use crate::types::Context;
use crate::types::ExposedInfo;
use crate::types::ExposedTypes;
use crate::types::InstanceData;
use crate::types::OptionalString;
use crate::types::ParamVar;
use crate::types::Parameters;
use crate::types::Seq;
use crate::types::ShardsVar;
use crate::types::Type;
use crate::types::Types;
use crate::types::Var;
use crate::types::ANYS_TYPES;
use crate::types::ANY_TYPES;
use crate::types::SHARDS_OR_NONE_TYPES;
use std::cmp::Ordering;
use std::ffi::CStr;

lazy_static! {
  static ref LISTBOX_PARAMETERS: Parameters = vec![
    (
      cstr!("Index"),
      shccstr!("The index of the selected item."),
      INT_VAR_OR_NONE_SLICE,
    )
      .into(),
    (
      cstr!("Template"),
      shccstr!("Custom rendering"),
      &SHARDS_OR_NONE_TYPES[..],
    )
      .into(),
  ];
}

impl Default for ListBox {
  fn default() -> Self {
    let mut parents = ParamVar::default();
    parents.set_name(PARENTS_UI_NAME);
    Self {
      parents,
      requiring: Vec::new(),
      index: ParamVar::default(),
      template: ShardsVar::default(),
      exposing: Vec::new(),
      should_expose: false,
      tmp: 0,
    }
  }
}

impl Shard for ListBox {
  fn registerName() -> &'static str
  where
    Self: Sized,
  {
    cstr!("UI.ListBox")
  }

  fn hash() -> u32
  where
    Self: Sized,
  {
    compile_time_crc32::crc32!("UI.ListBox-rust-0x20200101")
  }

  fn name(&mut self) -> &str {
    "UI.ListBox"
  }

  fn help(&mut self) -> OptionalString {
    OptionalString(shccstr!("A list selection."))
  }

  fn inputTypes(&mut self) -> &Types {
    &ANYS_TYPES
  }

  fn inputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!("A sequence of values."))
  }

  fn outputTypes(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn outputHelp(&mut self) -> OptionalString {
    OptionalString(shccstr!("The selected value."))
  }

  fn parameters(&mut self) -> Option<&Parameters> {
    Some(&LISTBOX_PARAMETERS)
  }

  fn setParam(&mut self, index: i32, value: &Var) -> Result<(), &str> {
    match index {
      0 => Ok(self.index.set_param(value)),
      1 => self.template.set_param(value),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.index.get_param(),
      1 => self.template.get_param(),
      _ => Var::default(),
    }
  }

  fn hasCompose() -> bool {
    true
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    if self.index.is_variable() {
      self.should_expose = true; // assume we expose a new variable

      let shared: ExposedTypes = data.shared.into();
      for var in shared {
        let (a, b) = unsafe {
          (
            CStr::from_ptr(var.name),
            CStr::from_ptr(self.index.get_name()),
          )
        };
        if CStr::cmp(a, b) == Ordering::Equal {
          self.should_expose = false;
          if var.exposedType.basicType != shardsc::SHType_Int {
            return Err("Combo: int variable required.");
          }
          break;
        }
      }
    }

    let input_type = data.inputType;
    let slice = unsafe {
      let ptr = input_type.details.seqTypes.elements;
      std::slice::from_raw_parts(ptr, input_type.details.seqTypes.len as usize)
    };
    let element_type = if !slice.is_empty() {
      slice[0]
    } else {
      common_type::none
    };

    if !self.template.is_empty() {
      let mut data = *data;
      data.inputType = element_type;
      self.template.compose(&data)?;
    }

    Ok(element_type)
  }

  fn exposedVariables(&mut self) -> Option<&ExposedTypes> {
    if self.index.is_variable() && self.should_expose {
      self.exposing.clear();

      let exp_info = ExposedInfo {
        exposedType: common_type::int,
        name: self.index.get_name(),
        help: cstr!("The exposed int variable").into(),
        ..ExposedInfo::default()
      };

      self.exposing.push(exp_info);
      Some(&self.exposing)
    } else {
      None
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

    self.index.warmup(ctx);
    if self.should_expose {
      self.index.get_mut().valueType = common_type::int.basicType;
    }

    if !self.template.is_empty() {
      self.template.warmup(ctx)?;
    }

    Ok(())
  }

  fn cleanup(&mut self) -> Result<(), &str> {
    self.template.cleanup();
    self.index.cleanup();
    self.parents.cleanup();

    Ok(())
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    if let Some(ui) = util::get_current_parent(self.parents.get())? {
      let index = &mut if self.index.is_variable() {
        let index: i64 = self.index.get().try_into()?;
        index as usize
      } else {
        self.tmp
      };
      let seq: Seq = input.try_into()?;

      let mut changed = false;
      ui.group(|ui| {
        for i in 0..seq.len() {
          if self.template.is_empty() {
            let str: &str = (&seq[i]).try_into()?;
            if ui.selectable_label(i == *index, str.to_owned()).clicked() {
              *index = i;
              changed = true;
            }
          } else {
            let inner_margin = egui::style::Margin::same(3.0);
            let background_id = ui.painter().add(egui::Shape::Noop);
            let outer_rect = ui.available_rect_before_wrap();
            let mut inner_rect = outer_rect;
            inner_rect.min += inner_margin.left_top();
            inner_rect.max -= inner_margin.right_bottom();
            // Make sure we don't shrink to the negative:
            inner_rect.max.x = inner_rect.max.x.max(inner_rect.min.x);
            inner_rect.max.y = inner_rect.max.y.max(inner_rect.min.y);

            let mut content_ui = ui.child_ui_with_id_source(inner_rect, *ui.layout(), i);
            util::activate_ui_contents(
              context,
              &seq[i],
              &mut content_ui,
              &mut self.parents,
              &mut self.template,
            )?;

            let mut paint_rect = content_ui.min_rect();
            paint_rect.min -= inner_margin.left_top();
            paint_rect.max += inner_margin.right_bottom();

            let selected = i == *index;
            let response = ui.allocate_rect(paint_rect, egui::Sense::click());
            let visuals = ui.style().interact_selectable(&response, selected);
            if selected || response.hovered() || response.has_focus() {
              let rect = paint_rect.expand(visuals.expansion);
              let shape = egui::Shape::Rect(egui::epaint::RectShape {
                rect,
                rounding: visuals.rounding,
                fill: visuals.bg_fill,
                stroke: visuals.bg_stroke,
              });
              ui.painter().set(background_id, shape);
            }

            if response.clicked() {
              *index = i;
              changed = true;
            }
          }
        }
        Ok::<(), &str>(())
      })
      .inner?;

      if *index >= seq.len() {
        // in fact if len == 0, we are fine to have -1 as a way to signal "no selection"
        *index = seq.len() - 1;
        changed = true;
      }

      if changed {
        if self.index.is_variable() {
          self.index.set_fast_unsafe(&(*index as i64).into());
        } else {
          self.tmp = *index;
        }
      }

      if seq.is_empty() {
        Ok(Var::default())
      } else {
        // this is fine because we don't own input and seq is just a view of it in this case
        Ok(seq[*index])
      }
    } else {
      Err("No UI parent")
    }
  }
}
