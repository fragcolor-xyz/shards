use crate::dnd::DragOp;
use crate::util;
use crate::util::with_possible_panic;
use crate::CONTEXTS_NAME;
use crate::PARENTS_UI_NAME;
use egui::Frame;
use egui::Id;
use egui::Rect;
use egui::Rgba;
use egui::Sense;
use egui::Stroke;
use egui::Vec2;
use shards::core::register_shard;
use shards::shard::Shard;
use shards::types::*;
use shards::types::{ANY_TYPES, BOOL_TYPES};

#[derive(shards::shard)]
#[shard_info(
  "UI.Selectable",
  "A wrapper that detects selection over the provided contents."
)]
struct Selectable {
  #[shard_param(
    "Contents",
    "The UI contents to wrap, contain, and detect for selection.",
    SHARDS_OR_NONE_TYPES
  )]
  contents: ShardsVar,
  #[shard_param(
    "IsSelected",
    "Callback function for checking if the contents are currently selected.",
    SHARDS_OR_NONE_TYPES
  )]
  is_selected_callback: ShardsVar,
  #[shard_param(
    "Clicked",
    "Callback function for the contents of this shard is clicked.",
    SHARDS_OR_NONE_TYPES
  )]
  clicked_callback: ShardsVar,
  #[shard_param(
    "DoubleClicked",
    "Callback function for the contents of this shard is clicked.",
    SHARDS_OR_NONE_TYPES
  )]
  double_clicked_callback: ShardsVar,
  #[shard_param(
    "ContextMenu",
    "Callback function for the right-click context menu.",
    SHARDS_OR_NONE_TYPES
  )]
  context_menu: ShardsVar,
  #[shard_param(
    "DragData",
    "Enables dragging and sets the data for drag operations",
    ANY_TYPES
  )]
  drag_data: ParamVar,
  #[shard_param(
    "ID",
    "An optional ID value in case of ID conflicts.",
    STRING_VAR_OR_NONE_SLICE
  )]
  id: ParamVar,
  #[shard_warmup]
  contexts: ParamVar,
  #[shard_warmup]
  parents: ParamVar,
  #[shard_required]
  required: ExposedTypes,
  inner_exposed: ExposedTypes,
  last_clicked: [Option<Id>; 2],
  last_size: Option<Vec2>,
}

impl Default for Selectable {
  fn default() -> Self {
    Self {
      contents: ShardsVar::default(),
      is_selected_callback: ShardsVar::default(),
      clicked_callback: ShardsVar::default(),
      double_clicked_callback: ShardsVar::default(),
      context_menu: ShardsVar::default(),
      drag_data: ParamVar::default(),
      id: ParamVar::default(),
      contexts: ParamVar::new_named(CONTEXTS_NAME),
      parents: ParamVar::new_named(PARENTS_UI_NAME),
      required: ExposedTypes::new(),
      inner_exposed: ExposedTypes::new(),
      last_clicked: [None, None],
      last_size: None,
    }
  }
}

#[shard_impl]
impl Shard for Selectable {
  fn input_types(&mut self) -> &Types {
    &ANY_TYPES
  }
  fn output_types(&mut self) -> &Types {
    &BOOL_TYPES
  }
  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;

    if self.contents.is_empty() {
      return Err("Contents are required");
    }

    let contents_composed = self.contents.compose(data)?;
    shards::util::merge_exposed_types(&mut self.inner_exposed, &contents_composed.exposedInfo);
    shards::util::merge_exposed_types(&mut self.required, &contents_composed.requiredInfo);

    let clicked_data = shards::SHInstanceData {
      inputType: common_type::any,
      ..*data
    };
    self.clicked_callback.compose(&clicked_data)?;
    shards::util::require_shards_contents(&mut self.required, &self.clicked_callback);

    self.double_clicked_callback.compose(&clicked_data)?;
    shards::util::require_shards_contents(&mut self.required, &self.double_clicked_callback);

    self.context_menu.compose(&clicked_data)?;
    shards::util::require_shards_contents(&mut self.required, &self.context_menu);

    let is_selected_data = shards::SHInstanceData {
      inputType: common_type::any,
      ..*data
    };
    let cr = self.is_selected_callback.compose(&is_selected_data)?;
    if cr.outputType != common_type::bool {
      return Err("IsSelected should return a boolean");
    }
    shards::util::require_shards_contents(&mut self.required, &self.is_selected_callback);

    Ok(contents_composed.outputType)
  }
  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.warmup_helper(context)?;
    Ok(())
  }
  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    Ok(())
  }
  fn exposed_variables(&mut self) -> Option<&ExposedTypes> {
    Some(&self.inner_exposed)
  }
  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    let ui = util::get_parent_ui(self.parents.get())?;
    let ui_ctx = util::get_current_context(&self.contexts)?;

    let id = if self.id.is_none() {
      ui.id().with(crate::EguiId::new(self, 0))
    } else {
      ui.id().with(TryInto::<&str>::try_into(self.id.get())?)
    };

    // Resolve IsSelected
    let mut is_selected_var = Var::default();
    if self
      .is_selected_callback
      .activate(context, &input, &mut is_selected_var)
      == WireState::Error
    {
      return Err("IsSelected callback failed");
    }
    let is_selected: bool = (&is_selected_var).try_into().unwrap();

    let style = ui.visuals().widgets.active;
    // Draw frame as either white or gray, depending on whether the contents are selected
    let stroke = if is_selected {
      Stroke {
        color: Rgba::WHITE.into(),
        ..style.bg_stroke
      }
    } else {
      Stroke {
        color: Rgba::from_gray(0.2).into(),
        ..style.bg_stroke
      }
    };

    // ui.interact(rect, id, sense)

    let drag_data = self.drag_data.get();
    let can_drag = !drag_data.is_none();

    let mut err: Option<&str> = None;
    ui.push_id(id, |ui| -> Result<(), &str> {
      let mut body = |ui: &mut egui::Ui| {
        // Draw frame and contents
        with_possible_panic(|| {
          Frame::group(ui.style())
            .stroke(stroke)
            .show(ui, |ui| {
              if let Err(e) = util::activate_ui_contents(
                context,
                input,
                ui,
                &mut self.parents,
                &mut self.contents,
              ) {
                err = Some(e);
              }
            })
            .response
        })
      };

      let inner_response = if let Some(ls) = self.last_size {
        let r = Rect::from_min_size(ui.cursor().min, ls);
        let sense = if can_drag {
          Sense::click_and_drag()
        } else {
          Sense::click()
        };
        let interact_id = id.with("inner");
        let interact_response = ui.interact(r, interact_id, sense);

        let inner_response = if can_drag {
          let mut op = DragOp::new(interact_id, ui, &ui_ctx);
          if op.is_dragging() {
            let layer_id = egui::LayerId::new(egui::Order::Tooltip, id);
            let inner = ui.with_layer_id(layer_id, body).inner?;
            op.update_dragging(layer_id, &interact_response, ui);
            inner
          } else {
            op.update_not_dragging(drag_data, &interact_response, ui, &ui_ctx);
            ui.scope(body).inner?
          }
        } else {
          body(ui)?
        };

        if !self.context_menu.is_empty() {
          interact_response.context_menu(|ui| {
            util::activate_ui_contents(
              context,
              input,
              ui,
              &mut self.parents,
              &mut self.context_menu,
            )
            .err();
          });
        }

        // Find clicks in other UI elements, and ignore selection response in that case
        let ignore_click = ui_ctx.override_selection_response.is_some();
        if interact_response.hovered() && !ignore_click {
          if ui.input(|i| {
            i.pointer
              .button_double_clicked(egui::PointerButton::Primary)
              || i
                .pointer
                .button_triple_clicked(egui::PointerButton::Primary)
          }) {
            if self.last_clicked[0] == self.last_clicked[1] {
              let mut _unused = Var::default();
              if self
                .double_clicked_callback
                .activate(context, &input, &mut _unused)
                == WireState::Error
              {
                return Err("DoubleClicked callback failed");
              }
            }
          } else if ui.input(|i| {
            (i.pointer.primary_released() && !i.modifiers.any())
              || (i.pointer.primary_clicked() && i.modifiers.any())
          }) {
            let mut _unused = Var::default();
            if self
              .clicked_callback
              .activate(context, &input, &mut _unused)
              == WireState::Error
            {
              return Err("Clicked callback failed");
            }
          }

          if ui.input(|i| i.pointer.primary_clicked()) {
            self.last_clicked[1] = self.last_clicked[0];
            self.last_clicked[0] = Some(id);
          }
        }

        inner_response
      } else {
        body(ui)?
      };

      self.last_size = Some(inner_response.rect.size());

      Ok(())
    });

    if let Some(err) = err {
      return Err(err);
    }

    // Outputs whether the contents are selected or not
    Ok(is_selected.into())
  }
}

pub fn register_shards() {
  register_shard::<Selectable>();
}
