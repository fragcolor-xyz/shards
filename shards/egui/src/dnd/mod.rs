use std::sync::Arc;

use crate::util;
use crate::EguiId;
use crate::CONTEXTS_NAME;
use crate::PARENTS_UI_NAME;
use egui::epaint;
use egui::InnerResponse;
use egui::Rect;
use egui::Shape;
use egui::Vec2;
use shards::core::register_shard;
use shards::shard::Shard;
use shards::types::*;

pub fn drag_source(ui: &mut egui::Ui, id: egui::Id, body: impl FnOnce(&mut egui::Ui)) {
  let is_being_dragged = ui.memory(|mem| mem.is_being_dragged(id));

  if !is_being_dragged {
    let response = ui.scope(body).response;

    // Check for drags:
    let response = ui.interact(response.rect, id, egui::Sense::drag());
    if response.hovered() {
      ui.ctx().set_cursor_icon(egui::CursorIcon::Grab);
    }
  } else {
    ui.ctx().set_cursor_icon(egui::CursorIcon::Grabbing);

    // Paint the body to a new layer:
    let layer_id = egui::LayerId::new(egui::Order::Tooltip, id);
    let response = ui.with_layer_id(layer_id, body).response;

    // Now we move the visuals of the body to where the mouse is.
    // Normally you need to decide a location for a widget first,
    // because otherwise that widget cannot interact with the mouse.
    // However, a dragged component cannot be interacted with anyway
    // (anything with `Order::Tooltip` always gets an empty [`Response`])
    // So this is fine!

    if let Some(pointer_pos) = ui.ctx().pointer_interact_pos() {
      let delta = pointer_pos - response.rect.center();
      ui.ctx().translate_layer(layer_id, delta);
    }
  }
}

pub fn drop_target<R>(
  ui: &mut egui::Ui,
  can_accept_what_is_being_dragged: bool,
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

  let mut fill = style.bg_fill;
  let mut stroke = style.bg_stroke;
  if is_being_dragged && !can_accept_what_is_being_dragged {
    fill = ui.visuals().gray_out(fill);
    stroke.color = ui.visuals().gray_out(stroke.color);
  }

  ui.painter().set(
    where_to_put_background,
    epaint::RectShape {
      rect,
      rounding: style.rounding,
      stroke,
      fill: fill.into(),
    },
  );

  InnerResponse::new(ret, response)
}

#[derive(shards::shard)]
#[shard_info("UI.DND", "Drag and drop source & target.")]
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
    "Callback function for when something is dropped.",
    SHARDS_OR_NONE_TYPES
  )]
  drop_callback: ShardsVar,
  #[shard_warmup]
  contexts: ParamVar,
  #[shard_warmup]
  parents: ParamVar,
  #[shard_required]
  required: ExposedTypes,
  inner_exposed: ExposedTypes,
  // A copy of the value being dragged
  payload: Option<Arc<ClonedVar>>,
}

impl Default for DragDrop {
  fn default() -> Self {
    Self {
      contents: ShardsVar::default(),
      hover_callback: ShardsVar::default(),
      drop_callback: ShardsVar::default(),
      contexts: ParamVar::new_named(CONTEXTS_NAME),
      parents: ParamVar::new_named(PARENTS_UI_NAME),
      required: ExposedTypes::new(),
      inner_exposed: ExposedTypes::new(),
      payload: None,
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
    &NONE_TYPES
  }
  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;

    self.inner_exposed.clear();
    self.contents.compose(data)?;
    shards::util::expose_shards_contents(&mut self.inner_exposed, &self.contents);
    shards::util::require_shards_contents(&mut self.required, &self.contents);

    self.drop_callback.compose(data)?;
    shards::util::require_shards_contents(&mut self.required, &self.drop_callback);

    self.hover_callback.compose(data)?;
    shards::util::require_shards_contents(&mut self.required, &self.hover_callback);

    Ok(NONE_TYPES[0])
  }
  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.warmup_helper(context)?;
    Ok(())
  }
  fn cleanup(&mut self) -> Result<(), &str> {
    self.cleanup_helper()?;
    Ok(())
  }
  fn exposed_variables(&mut self) -> Option<&ExposedTypes> {
    Some(&self.inner_exposed)
  }
  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    let ui = util::get_parent_ui(self.parents.get())?;

    let r = drag_source(ui, EguiId::new(self, 0).into(), |ui| {
      util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.contents)
        .expect("");
    });

    Ok(Var::default())
  }
}

pub fn register_shards() {
  register_shard::<DragDrop>();
}
