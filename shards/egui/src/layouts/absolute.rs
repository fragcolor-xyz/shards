use crate::{util, EguiId, CONTEXTS_NAME, EGUI_CTX_TYPE, HELP_VALUE_IGNORED, PARENTS_UI_NAME};
use egui::{Align2, LayerId, Pos2, Sense, Vec2};
use shards::{
  core::registerShard,
  shard::Shard,
  types::{
    common_type, ClonedVar, Context, ExposedInfo, ExposedTypes, InstanceData, OptionalString,
    ParamVar, Parameters, ShardsVar, Type, Types, Var, NONE_TYPES, SHARDS_OR_NONE_TYPES,
  },
};

shard! {
  struct UIAbsoluteShard("UI.Absolute", "desc") {
    #[Param("Contents", "", SHARDS_OR_NONE_TYPES)]
    pub contents: ShardsVar,
    #[Param("Rect", "The target rectangle (X/Y/W/H)", [common_type::float4, common_type::float4_var])]
    pub rect: ParamVar,
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
      let (x,y,w,h): (f32,f32,f32,f32) = self.rect.get().try_into()?;

      let ui_ctx = util::get_current_context(&self.contexts)?;
      let layer_id = LayerId::new(egui::Order::Foreground, EguiId::new(self, 1).into());
      let rect = egui::Rect::from_min_size(Pos2::new(x,y), Vec2::new(w,h));

      let mut frame = egui::Area::new(layer_id.id);
      frame = frame.fixed_pos(rect.min);

      // let result = frame.show(ui_ctx, |ui| {
      //   ui.set_max_size(rect.size());

      //   let painter = ui.painter();
      //   painter.rect_filled(ui.max_rect(), 0.5, egui::Color32::WHITE);
      //   util::activate_ui_contents(context, input, ui, &mut self.parents, &mut self.contents)
      // }).inner?;

      // println!("Wants pointer: {}", ui_ctx.wants_pointer_input());

      // let state = egui:: State

      let interact_id = layer_id.id.with("move");
      let mut ui = egui::Ui::new(ui_ctx.clone(), layer_id, layer_id.id, rect, ui_ctx.screen_rect());
      ui.set_enabled(true);
      let result = util::activate_ui_contents(context, input, &mut ui, &mut self.parents, &mut self.contents)?;
      let response = ui.interact(rect, interact_id, Sense::click());
      Ok(result)
    }
  }
}

impl Default for UIAbsoluteShard {
  fn default() -> Self {
    Self {
      contexts: ParamVar::new_named(CONTEXTS_NAME),
      parents: ParamVar::new_named(PARENTS_UI_NAME),
      rect: ParamVar::default(),
      required: Vec::new(),
      contents: ShardsVar::default(),
      inner_exposed: ExposedTypes::new(),
    }
  }
}

pub fn registerShards() {
  registerShard::<UIAbsoluteShard>();
}
