/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright © 2022 Fragcolor Pte. Ltd. */

use super::ListBox;
use crate::util;
use crate::INT_VAR_OR_NONE_SLICE;
use crate::PARENTS_UI_NAME;
use egui::Rect;
use shards::shard::LegacyShard;
use shards::shardsc;
use shards::shardsc::SHType_Bool;
use shards::types::common_type;
use shards::types::Context;
use shards::types::ExposedInfo;
use shards::types::ExposedTypes;
use shards::types::InstanceData;
use shards::types::OptionalString;
use shards::types::ParamVar;
use shards::types::Parameters;
use shards::types::Seq;
use shards::types::ShardsVar;
use shards::types::Type;
use shards::types::Types;
use shards::types::Var;
use shards::types::ANY_TYPES;

use shards::types::SHARDS_OR_NONE_TYPES;
use std::cmp::Ordering;
use std::ffi::CStr;

use shards::util::from_raw_parts_allow_null;

lazy_static! {
  static ref LISTBOX_PARAMETERS: Parameters = vec![
    (
      cstr!("Index"),
      shccstr!("The index of the selected item."),
      INT_VAR_OR_NONE_SLICE,
    )
      .into(),
    (
      cstr!("IsSelected"),
      shccstr!("Predicate that should return selection state of an item, receives the index in the list, should return true/false."),
      &SHARDS_OR_NONE_TYPES[..],
    )
    .into(),
    (
      cstr!("Clicked"),
      shccstr!("Action to perform if an element of the list is being clicked."),
      &SHARDS_OR_NONE_TYPES[..],
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
      is_selected: ShardsVar::default(),
      selected: ShardsVar::default(),
      exposing: Vec::new(),
      should_expose: false,
      tmp: 0,
    }
  }
}

impl LegacyShard for ListBox {
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
    &ANY_TYPES
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
      0 => self.index.set_param(value),
      1 => self.is_selected.set_param(value),
      2 => self.selected.set_param(value),
      3 => self.template.set_param(value),
      _ => Err("Invalid parameter index"),
    }
  }

  fn getParam(&mut self, index: i32) -> Var {
    match index {
      0 => self.index.get_param(),
      1 => self.is_selected.get_param(),
      2 => self.selected.get_param(),
      3 => self.template.get_param(),
      _ => Var::default(),
    }
  }

  fn hasCompose() -> bool {
    true
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.requiring.clear();

    // Add UI.Parents to the list of required variables
    util::require_parents(&mut self.requiring);

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

    if !self.is_selected.is_empty() {
      let mut data_copy = *data;
      data_copy.inputType = common_type::int;
      let res = self.is_selected.compose(&data_copy)?;

      if res.outputType.basicType != SHType_Bool {
        return Err("IsSelected callback should return a boolean");
      }

      for item in res.requiredInfo.iter() {
        self.requiring.push(item);
      }
    }

    if !self.selected.is_empty() {
      let mut data_copy = *data;
      data_copy.inputType = common_type::int;
      let res = self.selected.compose(&data_copy)?;

      for item in res.requiredInfo.iter() {
        self.requiring.push(item);
      }
    }

    let input_type = data.inputType;
    let slice = unsafe {
      let ptr = input_type.details.seqTypes.elements;
      from_raw_parts_allow_null(ptr, input_type.details.seqTypes.len as usize)
    };

    let element_type = match slice.len() {
      0 => common_type::none,
      1 => slice[0],
      _ => {
        if slice.iter().skip(1).all(|t| *t == slice[0]) {
          slice[0]
        } else {
          common_type::any
        }
      }
    };

    if !self.template.is_empty() {
      let mut data = *data;
      data.inputType = element_type;
      self.template.compose(&data)?;
    } else if element_type.basicType != shardsc::SHType_String {
      return Err("Input is not a sequence of strings, a template must be provided.");
    }

    Ok(element_type)
  }

  fn exposedVariables(&mut self) -> Option<&ExposedTypes> {
    if self.index.is_variable() && self.should_expose {
      self.exposing.clear();

      let exp_info = ExposedInfo {
        exposedType: common_type::int,
        name: self.index.get_name(),
        help: shccstr!("The exposed int variable"),
        declared: true,
        isMutable: true,
        ..ExposedInfo::default()
      };

      self.exposing.push(exp_info);
      Some(&self.exposing)
    } else {
      None
    }
  }

  fn requiredVariables(&mut self) -> Option<&ExposedTypes> {
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

    if !self.selected.is_empty() {
      self.selected.warmup(ctx)?;
    }

    if !self.is_selected.is_empty() {
      self.is_selected.warmup(ctx)?;
    }

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.template.cleanup(ctx);
    self.index.cleanup(ctx);
    self.parents.cleanup(ctx);
    self.selected.cleanup(ctx);
    self.is_selected.cleanup(ctx);

    Ok(())
  }

  fn activate(&mut self, context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    if let Some(parent) = util::get_current_parent_opt(self.parents.get())? {
      let current_index = if self.index.is_variable() {
        self.index.get().try_into()?
      } else {
        self.tmp
      };

      let mut new_index = None;

      let seq: Seq = input.try_into()?;

      if seq.is_empty() {
        return Ok(Some(Var::default()));
      }

      parent
        .group(|ui| {
          for i in 0..seq.len() {
            let is_selected = {
              if !self.is_selected.is_empty() {
                let input = Var::from(i as i64);
                let mut output = Var::default();
                self.is_selected.activate(context, &input, &mut output);
                <bool>::try_from(&output)?
              } else {
                i as i64 == current_index
              }
            };

            if self.template.is_empty() {
              let str: &str = (&seq[i]).try_into()?;
              if ui.selectable_label(is_selected, str.to_owned()).clicked() {
                if !self.selected.is_empty() {
                  let input = Var::from(i as i64);
                  let mut _output = Var::default();
                  self.selected.activate(context, &input, &mut _output);
                } else {
                  new_index = Some(i as i64);
                }
              }
            } else {
              let inner_margin = egui::Margin::same(3.0);
              let background_id = ui.painter().add(egui::Shape::Noop);
              let outer_rect = ui.available_rect_before_wrap();
              let mut inner_rect = outer_rect;
              inner_rect.min += inner_margin.left_top();
              inner_rect.max -= inner_margin.right_bottom();
              // Make sure we don't shrink to the negative:
              inner_rect.max.x = inner_rect.max.x.max(inner_rect.min.x);
              inner_rect.max.y = inner_rect.max.y.max(inner_rect.min.y);

              let mut content_ui = ui.child_ui_with_id_source(inner_rect, *ui.layout(), i, None);
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

              let response = ui.allocate_rect(paint_rect, egui::Sense::click());
              let visuals = ui.style().interact_selectable(&response, is_selected);
              if is_selected || response.hovered() || response.has_focus() {
                let rect = paint_rect.expand(visuals.expansion);
                let shape = egui::Shape::Rect(egui::epaint::RectShape {
                  rect,
                  rounding: visuals.rounding,
                  fill: visuals.bg_fill,
                  stroke: visuals.bg_stroke,
                  fill_texture_id: egui::TextureId::Managed(0),
                  uv: Rect::ZERO,
                  blur_width: 0.0,
                });
                ui.painter().set(background_id, shape);
              }

              if response.clicked() {
                if !self.selected.is_empty() {
                  let input = Var::from(i as i64);
                  let mut _output = Var::default();
                  self.selected.activate(context, &input, &mut _output);
                } else {
                  new_index = Some(i as i64);
                }
              }
            }
          }
          Ok::<(), &str>(())
        })
        .inner?;

      let current_index = if let Some(new_index) = new_index {
        new_index
      } else {
        current_index
      };

      if current_index >= seq.len() as i64 || current_index < 0 {
        // also fixup the index if it's out of bounds
        let fixup = -1i64;
        if self.index.is_variable() {
          self.index.set_fast_unsafe(&fixup.into());
        } else {
          self.tmp = fixup;
        }

        Ok(Some(Var::default()))
      } else {
        if self.index.is_variable() {
          self.index.set_fast_unsafe(&current_index.into());
        } else {
          self.tmp = current_index;
        }

        // this is fine because we don't own input and seq is just a view of it in this case
        Ok(Some(seq[current_index as usize]))
      }
    } else {
      Err("No UI parent")
    }
  }
}
