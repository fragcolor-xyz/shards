use std::f32::consts::E;

use crate::{
  util, Anchor, EguiId, UIOrder, CONTEXTS_NAME, EGUI_CTX_TYPE, HELP_VALUE_IGNORED, PARENTS_UI_NAME,
};
use egui::{
  epaint::RectShape, Align2, Color32, LayerId, Pos2, Rgba, Rounding, Sense, Shape, Stroke, Vec2,
};
use shards::{
  core::registerShard,
  shard::Shard,
  types::{
    common_type, ClonedVar, Context, ExposedInfo, ExposedTypes, InstanceData, OptionalString,
    ParamVar, Parameters, ShardsVar, Type, Types, Var, NONE_TYPES, SHARDS_OR_NONE_TYPES,
  },
};

lazy_static! {
  static ref ANCHOR_VAR_TYPE: Type = Type::context_variable(&crate::ANCHOR_TYPES);
}

shard! {
  struct UIAbsoluteShard("UI.Absolute", "desc") {
    #[Param("Contents", "", SHARDS_OR_NONE_TYPES)]
    pub contents: ShardsVar,
    #[Param("Position", "The target UI position (X/Y)", [common_type::float2, common_type::float2_var])]
    pub position: ParamVar,
    #[Param("Pivot", "The pivot for the inner UI", [*crate::ANCHOR_TYPE, *ANCHOR_VAR_TYPE])]
    pub pivot: ParamVar,
    #[Param("Anchor", "The anchored position", [*crate::ANCHOR_TYPE, *ANCHOR_VAR_TYPE])]
    pub anchor: ParamVar,
    #[Param("Order", "The order this UI is drawn in", [*crate::UIORDER_TYPE])]
    pub order: ParamVar,
    contexts: ParamVar,
    parents: ParamVar,
    inner_exposed: ExposedTypes,
  }

  impl Shard for TestMacroShard2 {
    fn inputTypes(&mut self) -> &Types {
      &NONE_TYPES
    }
    fn outputTypes(&mut self) -> &Types {
      &NONE_TYPES
    }
    fn inputHelp(&mut self) -> OptionalString {
      *HELP_VALUE_IGNORED
    }
    fn warmup(&mut self, context: &Context) -> Result<(), &str> {
      self.warmup_helper(context)?;
      self.contexts.warmup(context);
      self.parents.warmup(context);
      Ok(())
    }
    fn cleanup(&mut self) -> Result<(), &str> {
      self.cleanup_helper()?;
      self.contexts.cleanup();
      self.parents.cleanup();
      Ok(())
    }
    fn exposedVariables(&mut self) -> Option<&ExposedTypes> {
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
      let (x,y): (f32,f32) = self.position.get().try_into()?;

      let ui_ctx = util::get_current_context(&self.contexts)?;

      let mut frame = egui::Area::new(EguiId::new(self, 1));
      
      frame = frame.order(if let Ok(ev) = self.order.get().enum_value() {
        UIOrder{bits: ev}.try_into()?
      } else {
        egui::Order::PanelResizeLine
      });

      frame = frame.pivot(if let Ok(ev) = self.pivot.get().enum_value()  {
        Anchor{bits: ev}.try_into()?
      } else {
        egui::Align2::LEFT_TOP
      });

      // Either anchor or fix size
      if let Ok(ev) = self.anchor.get().enum_value() {
        frame = frame.anchor(Anchor{bits: ev}.try_into()?, Vec2::new(x, y));
      } else {
        frame = frame.fixed_pos(Pos2::new(x, y));
      }

      let result = frame.show(ui_ctx, |ui| {
        // ui.set_max_size(rect.size());

        let where_to_put_bg = ui.painter().add(Shape::Noop);

        match util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.contents) {
          Ok(result) => {
            let painter = ui.painter();
            let bg_shape = Shape::Rect(RectShape{
              rect: ui.min_rect().expand(5.0),
              fill: Rgba::from_white_alpha(0.2).into(),
              rounding: Rounding::none(),
              stroke: Stroke::new(3.0, Color32::YELLOW),
            });
            painter.set(where_to_put_bg, bg_shape);
            Ok(result)
          },
          Err(e) => {
            Err(e)
          }
        }

      }).inner?;

      println!("Wants pointer: {}", ui_ctx.wants_pointer_input());
      Ok(result)
    }
  }
}

impl Default for UIAbsoluteShard {
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

pub fn registerShards() {
  registerShard::<UIAbsoluteShard>();
}
