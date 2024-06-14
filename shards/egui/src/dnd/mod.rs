use std::sync::Arc;

use crate::bindings::SHInstanceData;
use crate::util;
use crate::Context as UIContext;
use crate::EguiId;
use crate::CONTEXTS_NAME;
use crate::PARENTS_UI_NAME;
use egui::epaint;
use egui::InnerResponse;
use egui::Rect;
use egui::Rgba;
use egui::Shape;
use egui::Stroke;
use egui::Vec2;
use shards::core::register_shard;
use shards::shard::Shard;
use shards::types::*;

const DRAG_THRESHOLD: f32 = 15.0;

pub fn drag_source<R>(
  ui: &mut egui::Ui,
  ctx: &UIContext,
  payload: &Var,
  drag_size: Vec2,
  id: egui::Id,
  body: impl FnOnce(&mut egui::Ui) -> R,
) -> InnerResponse<R> {
  let is_dropped = ui.input(|i| i.pointer.any_released());
  let is_dragging_something = !ctx.dnd_value.borrow().0.is_none();
  let is_being_dragged = is_dragging_something && ui.memory(|mem| mem.is_being_dragged(id));

  let cursor_already_set =
    is_dropped || ui.output(|x| x.cursor_icon == egui::CursorIcon::NotAllowed);

  if !is_being_dragged {
    let rect = Rect::from_min_size(ui.cursor().min, drag_size);

    // Check for drags:
    let response = ui.interact(rect, id, egui::Sense::drag());
    if response.hovered() && !cursor_already_set {
      ui.ctx().set_cursor_icon(egui::CursorIcon::Grab);
    }

    // Store the drag payload
    if response.dragged() {
      let delta = ui.input(|i| {
        if let Some(origin) = i.pointer.press_origin() {
          let b = i.pointer.interact_pos().unwrap_or_default();
          Some(b - origin)
        } else {
          None
        }
      });
      if let Some(delta) = delta {
        if delta.length() > DRAG_THRESHOLD {
          ctx.dnd_value.borrow_mut().assign(payload);
        }
      }
    }

    let inner = ui.scope(body);

    inner
  } else {
    if !cursor_already_set {
      ui.ctx().set_cursor_icon(egui::CursorIcon::Grabbing);
    }

    // Paint the body to a new layer:
    let layer_id = egui::LayerId::new(egui::Order::Tooltip, id);
    let inner = ui.with_layer_id(layer_id, body);
    let response = &inner.response;

    // Now we move the visuals of the body to where the mouse is.
    // Normally you need to decide a location for a widget first,
    // because otherwise that widget cannot interact with the mouse.
    // However, a dragged component cannot be interacted with anyway
    // (anything with `Order::Tooltip` always gets an empty [`Response`])
    // So this is fine!

    if is_dragging_something {
      if let Some(pointer_pos) = ui.ctx().pointer_interact_pos() {
        let delta = pointer_pos - response.rect.center();
        ui.ctx().translate_layer(layer_id, delta);
      }
    }

    inner
  }
}

pub fn drop_target<R>(
  ui: &mut egui::Ui,
  ctx: &UIContext,
  can_accept_what_is_being_dragged: bool,
  visualize: bool,
  body: impl FnOnce(&mut egui::Ui) -> R,
) -> InnerResponse<R> {
  let is_being_dragged = ui.memory(|mem| mem.is_anything_being_dragged());

  let margin = Vec2::splat(4.0);

  let outer_rect_bounds = ui.available_rect_before_wrap();
  let inner_rect = outer_rect_bounds.shrink2(margin);
  let where_to_put_background = ui.painter().add(Shape::Noop);
  let mut content_ui = ui.child_ui(inner_rect, *ui.layout());
  let ret = body(&mut content_ui);
  let outer_rect = Rect::from_min_max(outer_rect_bounds.min, content_ui.min_rect().max + margin);
  let (rect, response) = ui.allocate_at_least(outer_rect.size(), egui::Sense::hover());

  let style = if is_being_dragged && can_accept_what_is_being_dragged && response.hovered() {
    ui.visuals().widgets.active
  } else {
    ui.visuals().widgets.inactive
  };

  if is_being_dragged && !ctx.dnd_value.borrow().0.is_none() {
    let style = ui.visuals().widgets.active;
    let mut fill: Rgba = style.bg_fill.into();
    let mut stroke_color = style.bg_stroke.color.into();
    if !can_accept_what_is_being_dragged {
      fill = ui.visuals().gray_out(fill.into()).into();
      stroke_color = Rgba::RED;

      if response.hovered() {
        ui.ctx().set_cursor_icon(egui::CursorIcon::NotAllowed);
      }
    } else {
      (fill, stroke_color) = if response.hovered() {
        (fill * 1.5, Rgba::WHITE)
      } else {
        (fill, Rgba::from_gray(0.6))
      };
    }

    if visualize {
      let mut stroke = Stroke {
        color: stroke_color.into(),
        ..style.bg_stroke
      };

      ui.painter().set(
        where_to_put_background,
        epaint::RectShape {
          rect,
          rounding: style.rounding,
          stroke,
          fill: fill.into(),
          fill_texture_id: egui::TextureId::Managed(0),
          uv: Rect::ZERO,
        },
      );
    }
  }

  InnerResponse::new(ret, response)
}

#[derive(shards::shard)]
#[shard_info("UI.DragAndDrop", "Drag and drop source & target.")]
struct DragDrop {
  #[shard_param(
    "Contents",
    "The property to retrieve from the UI context.",
    SHARDS_OR_NONE_TYPES
  )]
  contents: ShardsVar,
  #[shard_param(
    "Hover",
    "Callback function for checking if this is a valid drop target.",
    SHARDS_OR_NONE_TYPES
  )]
  hover_callback: ShardsVar,
  #[shard_param(
    "Drop",
    "Callback function for when something is dropped. When set, this element will act as a drop target. When unset, it will act as a drag source.",
    SHARDS_OR_NONE_TYPES
  )]
  drop_callback: ShardsVar,
  #[shard_param(
    "ID",
    "An optional ID value in case of ID conflicts.",
    STRING_VAR_OR_NONE_SLICE
  )]
  id: ParamVar,
  #[shard_param("Visualize", "Visualize valid drop targets", BOOL_OR_NONE_SLICE)]
  visualize: ClonedVar,
  #[shard_warmup]
  contexts: ParamVar,
  #[shard_warmup]
  parents: ParamVar,
  #[shard_required]
  required: ExposedTypes,
  inner_exposed: ExposedTypes,
  // A copy of the value being dragged
  payload: Option<Arc<ClonedVar>>,
  prev_size: Vec2,
}

impl Default for DragDrop {
  fn default() -> Self {
    Self {
      contents: ShardsVar::default(),
      hover_callback: ShardsVar::default(),
      drop_callback: ShardsVar::default(),
      id: ParamVar::default(),
      contexts: ParamVar::new_named(CONTEXTS_NAME),
      parents: ParamVar::new_named(PARENTS_UI_NAME),
      required: ExposedTypes::new(),
      inner_exposed: ExposedTypes::new(),
      visualize: ClonedVar::default(),
      payload: None,
      prev_size: Vec2::ZERO,
    }
  }
}

impl DragDrop {
  fn is_drop_target(&self) -> bool {
    !self.drop_callback.is_empty()
  }
}

#[shard_impl]
impl Shard for DragDrop {
  fn input_types(&mut self) -> &Types {
    &ANY_TYPES
  }
  fn output_types(&mut self) -> &Types {
    &ANY_TYPES
  }
  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.inner_exposed.clear();
    self.compose_helper(data)?;

    if self.id.is_variable() {
      let id_info = ExposedInfo {
        exposedType: common_type::string,
        name: self.id.get_name(),
        help: shccstr!("The ID variable."),
        ..ExposedInfo::default()
      };
      self.required.push(id_info);
    }

    if self.contents.is_empty() {
      return Err("Contents are required");
    }

    let contents_composed = self.contents.compose(data)?;
    shards::util::merge_exposed_types(&mut self.inner_exposed, &contents_composed.exposedInfo);
    shards::util::merge_exposed_types(&mut self.required, &contents_composed.requiredInfo);

    let drop_data = shards::SHInstanceData {
      inputType: common_type::any,
      ..*data
    };
    self.drop_callback.compose(&drop_data)?;
    shards::util::require_shards_contents(&mut self.required, &self.drop_callback);

    if !self.hover_callback.is_empty() {
      let hover_data = shards::SHInstanceData {
        inputType: common_type::any,
        ..*data
      };
      let cr = self.hover_callback.compose(&hover_data)?;
      if cr.outputType != common_type::bool {
        return Err("Hover should return a boolean");
      }
      shards::util::require_shards_contents(&mut self.required, &self.hover_callback);
    }

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

    Ok(if self.is_drop_target() {
      let accepts_value = if !self.hover_callback.is_empty() {
        let cb_input: Var = ui_ctx.dnd_value.borrow().0;
        if cb_input.is_none() {
          false
        } else {
          let mut accept_value_var = Var::default();
          if self
            .hover_callback
            .activate(context, &cb_input, &mut accept_value_var)
            == WireState::Error
          {
            return Err("Hover callback failed");
          }
          (&accept_value_var).try_into().unwrap()
        }
      } else {
        true
      };

      let visualize = TryInto::<bool>::try_into(&self.visualize.0).unwrap_or(true);

      let inner = drop_target(ui, ui_ctx, accepts_value, visualize, |ui| {
        util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.contents)
      });

      let is_being_dragged = ui.memory(|mem| mem.is_anything_being_dragged());
      if ui.input(|i| i.pointer.any_released()) {
        // Reset cursor
        ui.ctx().set_cursor_icon(egui::CursorIcon::Default);
        if is_being_dragged && accepts_value && inner.response.hovered() {
          let cb_input = ui_ctx.dnd_value.take();
          let mut _unused = Var::default();
          if self
            .drop_callback
            .activate(context, &cb_input.0, &mut _unused)
            == WireState::Error
          {
            return Err("Drop callback failed");
          }
        }
      }

      inner.inner?
    } else {
      let id = ui.id().with("dragdrop");
      let inner = drag_source(ui, ui_ctx, &input, self.prev_size, id, |ui| {
        let r =
          util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.contents);
        self.prev_size = ui.min_size();
        r
      });

      inner.inner?
    })
  }
}

pub fn register_shards() {
  register_shard::<DragDrop>();
}
