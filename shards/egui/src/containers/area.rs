use crate::{
  util::{self, with_possible_panic},
  Anchor, EguiId, Order, CONTEXTS_NAME, PARENTS_UI_NAME,
};
use egui::{Pos2, Rect, Vec2};
use shards::{
  core::register_shard,
  shard::Shard,
  types::{
    common_type, Context, ExposedTypes, InstanceData, OptionalString, ParamVar, ShardsVar, Type,
    Types, Var, ANY_TYPES, SHARDS_OR_NONE_TYPES,
  },
};

lazy_static! {
  static ref ANCHOR_VAR_TYPE: Type = Type::context_variable(&crate::ANCHOR_TYPES);
}

#[derive(shard)]
#[shard_info("UI.Area", "Places UI element at a specific position.")]
struct AreaShard {
  #[shard_param("Contents", "The UI contents.", SHARDS_OR_NONE_TYPES)]
  pub contents: ShardsVar,
  #[shard_param("Position", "Absolute UI position; or when anchor is set, relative offset. (X/Y)", [common_type::float2, common_type::float2_var])]
  pub position: ParamVar,
  #[shard_param("Pivot", "The pivot for the inner UI", [*crate::ANCHOR_TYPE, *ANCHOR_VAR_TYPE])]
  pub pivot: ParamVar,
  #[shard_param("Anchor", "Side of the screen to anchor the UI to.", [*crate::ANCHOR_TYPE, *ANCHOR_VAR_TYPE])]
  pub anchor: ParamVar,
  #[shard_param("Order", "Paint layer to be used for this UI. Default is background", [*crate::ORDER_TYPE])]
  pub order: ParamVar,
  contexts: ParamVar,
  parents: ParamVar,
  inner_exposed: ExposedTypes,
  #[shard_required]
  required: ExposedTypes,
}

impl Default for AreaShard {
  fn default() -> Self {
    Self {
      contexts: ParamVar::new_named(CONTEXTS_NAME),
      parents: ParamVar::new_named(PARENTS_UI_NAME),
      position: ParamVar::default(),
      pivot: ParamVar::default(),
      anchor: ParamVar::default(),
      order: ParamVar::default(),
      required: Vec::new(),
      contents: ShardsVar::default(),
      inner_exposed: ExposedTypes::new(),
    }
  }
}

#[shard_impl]
impl Shard for AreaShard {
  fn input_types(&mut self) -> &Types {
    &ANY_TYPES
  }
  fn output_types(&mut self) -> &Types {
    &ANY_TYPES
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
    let output_type = self.contents.compose(data)?.outputType;
    shards::util::expose_shards_contents(&mut self.inner_exposed, &self.contents);
    shards::util::require_shards_contents(&mut self.required, &self.contents);

    util::require_parents(&mut self.required);
    util::require_context(&mut self.required);

    Ok(output_type)
  }
  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    let (x, y): (f32, f32) = self.position.get().try_into().unwrap_or_default();

    let ui_ctx = &util::get_current_context(&self.contexts)?.egui_ctx;

    let mut frame = egui::Area::new(EguiId::new(self, 1).into());

    let order: Option<Order> = self.order.get().try_into().ok();
    frame = frame.order(order.unwrap_or(Order::Background).into());

    let pivot: Option<Anchor> = self.pivot.get().try_into().ok();
    frame = frame.pivot(pivot.unwrap_or(Anchor::TopLeft).into());

    // Either anchor or fix size
    if let Some(anchor) = self.anchor.get().try_into().ok() as Option<Anchor> {
      frame = frame.anchor(anchor.into(), Vec2::new(x, y));
    } else {
      frame = frame.fixed_pos(Pos2::new(x, y));
    }

    let result = with_possible_panic(|| {
      frame
        .show(ui_ctx, |ui| {
          util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.contents)
        })
        .inner
    })??;

    Ok(result)
  }
}

#[derive(shard)]
#[shard_info("UI.SubArea", "Places UI element at a specific position.")]
struct SubAreaShard {
  #[shard_param("Contents", "The UI contents.", SHARDS_OR_NONE_TYPES)]
  pub contents: ShardsVar,
  #[shard_param("Offset", "Relative offset. (X/Y)", [common_type::float2, common_type::float2_var])]
  pub offset: ParamVar,
  #[shard_param("Anchor", "The anchor for the inner UI, relative to the available space", [*crate::ANCHOR_TYPE, *ANCHOR_VAR_TYPE])]
  pub anchor: ParamVar,
  #[shard_param("Pivot", "The pivot for the inner UI", [*crate::ANCHOR_TYPE, *ANCHOR_VAR_TYPE])]
  pub pivot: ParamVar,
  parents: ParamVar,
  inner_exposed: ExposedTypes,
  #[shard_required]
  required: ExposedTypes,
  last_size: Option<Vec2>,
}

impl Default for SubAreaShard {
  fn default() -> Self {
    Self {
      parents: ParamVar::new_named(PARENTS_UI_NAME),
      offset: ParamVar::default(),
      anchor: ParamVar::default(),
      pivot: ParamVar::default(),
      contents: ShardsVar::default(),
      inner_exposed: ExposedTypes::new(),
      required: ExposedTypes::new(),
      last_size: None,
    }
  }
}

#[shard_impl]
impl Shard for SubAreaShard {
  fn input_types(&mut self) -> &Types {
    &ANY_TYPES
  }
  fn output_types(&mut self) -> &Types {
    &ANY_TYPES
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
  fn exposed_variables(&mut self) -> Option<&ExposedTypes> {
    Some(&self.inner_exposed)
  }
  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;

    util::require_parents(&mut self.required);

    self.inner_exposed.clear();
    let output_type = self.contents.compose(data)?.outputType;
    shards::util::expose_shards_contents(&mut self.inner_exposed, &self.contents);
    shards::util::require_shards_contents(&mut self.required, &self.contents);

    Ok(output_type)
  }
  fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
    let parent_ui = util::get_parent_ui(&self.parents.get())?;
    let (ox, oy): (f32, f32) = self.offset.get().try_into().unwrap_or_default();

    // Either anchor or fix size
    let anchor = if let Some(anchor) = self.anchor.get().try_into().ok() as Option<Anchor> {
      anchor.into()
    } else {
      egui::Align2::LEFT_TOP
    };

    let pivot = if let Some(pivot) = self.pivot.get().try_into().ok() as Option<Anchor> {
      pivot.into()
    } else {
      egui::Align2::LEFT_TOP
    };

    let container_rect = parent_ui.max_rect();
    let aligned = if let Some(last_size) = self.last_size {
      let anchored = anchor.align_size_within_rect(Vec2::ZERO, container_rect);
      let pivot_offset = pivot.anchor_rect(Rect::from_min_max(Pos2::ZERO, last_size.to_pos2()));
      Rect::from_min_size(
        anchored.min + pivot_offset.min.to_vec2() + Vec2::new(ox, oy),
        container_rect.size(),
      )
    } else {
      container_rect
    };

    let mut ui = parent_ui.child_ui(aligned, parent_ui.layout().clone());

    let result = util::activate_ui_contents(
      context,
      input,
      &mut ui,
      &mut self.parents,
      &mut self.contents,
    )?;

    self.last_size = Some(ui.min_size());

    Ok(result)
  }
}

pub fn register_shards() {
  register_shard::<AreaShard>();
  register_shard::<SubAreaShard>();
}
