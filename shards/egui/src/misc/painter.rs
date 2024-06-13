use crate::util::{self, try_into_color};
use egui::{Color32, LayerId, Pos2, Rect, Rgba, Rounding, Stroke, Ui, Vec2};
use shards::core::register_shard;
use shards::types::{
  common_type, Context, ExposedTypes, InstanceData, OptionalString, ParamVar, ShardsVar, Type,
  Types, Var, FLOAT_TYPES, NONE_TYPES, SHARDS_OR_NONE_TYPES,
};

use crate::shards::shard;
use crate::shards::shard::Shard;
use crate::{EguiId, Order, CONTEXTS_NAME, HELP_VALUE_IGNORED, PARENTS_UI_NAME};
use shards::types::COLOR_TYPES;
use shards::types::FLOAT2_TYPES;

lazy_static! {
  pub static ref COLOR_VAR_TYPES: Types = vec![common_type::color, common_type::color_var];
  pub static ref FLOAT2_VAR_TYPES: Types = vec![common_type::float2, common_type::float2_var];
  pub static ref FLOAT_VAR_TYPES: Types = vec![common_type::float, common_type::float_var];
}

#[derive(shard)]
#[shard_info("UI.Canvas", "A canvas to draw UI elements on")]
struct CanvasShard {
  #[shard_param("Contents", "", SHARDS_OR_NONE_TYPES)]
  pub contents: ShardsVar,
  #[shard_param("Rect", "The target UI position (X/Y/W/H)", [common_type::float4, common_type::float4_var])]
  pub rect: ParamVar,
  #[shard_param("Order", "The order this UI is drawn in", crate::ORDER_TYPES)]
  pub order: ParamVar,
  contexts: ParamVar,
  parents: ParamVar,
  inner_exposed: ExposedTypes,
  #[shard_required]
  required: ExposedTypes,
}

impl Default for CanvasShard {
  fn default() -> Self {
    Self {
      contexts: ParamVar::new_named(CONTEXTS_NAME),
      parents: ParamVar::new_named(PARENTS_UI_NAME),
      rect: ParamVar::default(),
      order: ParamVar::default(),
      required: Vec::new(),
      contents: ShardsVar::default(),
      inner_exposed: ExposedTypes::new(),
    }
  }
}

#[shard_impl]
impl Shard for CanvasShard {
  fn input_types(&mut self) -> &Types {
    &NONE_TYPES
  }
  fn output_types(&mut self) -> &Types {
    &NONE_TYPES
  }
  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.warmup_helper(context)?;
    self.contexts.warmup(context);
    self.parents.warmup(context);
    Ok(())
  }
  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    self.contexts.cleanup(ctx);
    self.parents.cleanup(ctx);
    Ok(())
  }
  fn exposed_variables(&mut self) -> Option<&ExposedTypes> {
    Some(&self.inner_exposed)
  }
  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;

    self.inner_exposed.clear();
    self.contents.compose(data)?;
    shards::util::expose_shards_contents(&mut self.inner_exposed, &self.contents);
    shards::util::require_shards_contents(&mut self.required, &self.contents);

    Ok(NONE_TYPES[0])
  }
  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    let (x, y, w, h): (f32, f32, f32, f32) = self.rect.get().try_into()?;

    let egui_ctx = &util::get_current_context(&self.contexts)?.egui_ctx;

    let order = self.order.get().try_into().unwrap_or(Order::Background);
    let layer_id = LayerId::new(order.into(), EguiId::new(self, 0).into());

    let mut ui = Ui::new(
      egui_ctx.clone(),
      layer_id,
      layer_id.id,
      egui::Rect::from_min_size(Pos2::new(x, y), Vec2::new(w, h)),
      Rect::EVERYTHING,
    );

    util::activate_ui_contents(
      context,
      input,
      &mut ui,
      &mut self.parents,
      &mut self.contents,
    )
  }
}

#[derive(shard)]
#[shard_info("UI.PaintCircle", "Draw a circle on the canvas")]
struct CircleShard {
  #[shard_param("Center", "Center of the circle", FLOAT2_VAR_TYPES)]
  pub center: ParamVar,
  #[shard_param("Radius", "Radius of the circle", FLOAT_VAR_TYPES)]
  pub radius: ParamVar,
  #[shard_param("StrokeWidth", "Width of circle outline", FLOAT_VAR_TYPES)]
  pub stroke_width: ParamVar,
  #[shard_param("StrokeColor", "Color of circle outline", COLOR_VAR_TYPES)]
  pub stroke_color: ParamVar,
  parents: ParamVar,
  #[shard_required]
  required: ExposedTypes,
}

impl Default for CircleShard {
  fn default() -> Self {
    Self {
      parents: shards::types::ParamVar::new_named(crate::PARENTS_UI_NAME),
      required: Vec::new(),
      center: ParamVar::default(),
      radius: ParamVar::default(),
      stroke_width: ParamVar::default(),
      stroke_color: ParamVar::default(),
    }
  }
}

#[shard_impl]
impl Shard for CircleShard {
  fn input_types(&mut self) -> &Types {
    &NONE_TYPES
  }
  fn output_types(&mut self) -> &Types {
    &NONE_TYPES
  }
  fn input_help(&mut self) -> OptionalString {
    *HELP_VALUE_IGNORED
  }
  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.warmup_helper(context)?;
    self.parents.warmup(context);
    Ok(())
  }
  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    self.parents.cleanup(ctx);
    Ok(())
  }
  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    util::require_parents(&mut self.required);
    if self.center.is_none() {
      return Err("Center is required");
    }
    if self.radius.is_none() {
      return Err("Radius is required");
    }
    Ok(NONE_TYPES[0])
  }
  fn activate(&mut self, _context: &Context, _input: &Var) -> Result<Var, &str> {
    let ui = util::get_parent_ui(self.parents.get())?;
    let painter = ui.painter();
    let (cx, cy): (f32, f32) = self.center.get().try_into()?;
    let rx: f32 = self.radius.get().try_into()?;
    let stroke_width: f32 = if let Ok(x) = self.stroke_width.get().try_into() {
      x
    } else {
      1.0
    };
    let stroke_color: egui::Color32 = if let Ok(x) = try_into_color(self.stroke_color.get()) {
      x
    } else {
      Rgba::WHITE.into()
    };
    painter.circle_stroke(
      egui::Pos2::new(cx, cy),
      rx,
      Stroke::new(stroke_width, stroke_color),
    );
    Ok(Var::default())
  }
}

pub fn register_shards() {
  register_shard::<CanvasShard>();
  register_shard::<CircleShard>();
}
