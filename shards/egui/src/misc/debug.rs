use egui::CentralPanel;
use shards::core::{register_enum, register_shard};
use shards::shard::Shard;
use shards::types::{common_type, ANY_TYPES};
use shards::types::{Context, ExposedTypes, InstanceData, ParamVar, Type, Types, Var};

use crate::{util, CONTEXTS_NAME, PARENTS_UI_NAME};

#[derive(shards::shard)]
#[shard_info("UI.Inspection", "Show inspection ui")]
struct InspectionShard {
  #[shard_warmup]
  contexts: ParamVar,
  #[shard_warmup]
  parents: ParamVar,
  #[shard_required]
  required: ExposedTypes,
}

impl Default for InspectionShard {
  fn default() -> Self {
    Self {
      contexts: ParamVar::new_named(CONTEXTS_NAME),
      parents: ParamVar::new_named(PARENTS_UI_NAME),
      required: ExposedTypes::new(),
    }
  }
}

#[shards::shard_impl]
impl Shard for InspectionShard {
  fn input_types(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;
    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;

    util::require_context(&mut self.required);
    util::require_parents(&mut self.required);
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, _input: &Var) -> Result<Option<Var>, &str> {
    let ui = util::get_parent_ui(self.parents.get())?;
    let ui_ctx = util::get_current_context(&self.contexts)?;

    ui_ctx.egui_ctx.inspection_ui(ui);
    Ok(None)
  }
}

#[derive(shards::shard)]
#[shard_info("UI.Settings", "Show settings ui")]
struct SettingsShard {
  #[shard_warmup]
  contexts: ParamVar,
  #[shard_warmup]
  parents: ParamVar,
  #[shard_required]
  required: ExposedTypes,
}

impl Default for SettingsShard {
  fn default() -> Self {
    Self {
      contexts: ParamVar::new_named(CONTEXTS_NAME),
      parents: ParamVar::new_named(PARENTS_UI_NAME),
      required: ExposedTypes::new(),
    }
  }
}

#[shards::shard_impl]
impl Shard for SettingsShard {
  fn input_types(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;
    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;

    util::require_context(&mut self.required);
    util::require_parents(&mut self.required);
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, _input: &Var) -> Result<Option<Var>, &str> {
    let ui = util::get_parent_ui(self.parents.get())?;
    let ui_ctx = util::get_current_context(&self.contexts)?;

    ui_ctx.egui_ctx.settings_ui(ui);
    Ok(None)
  }
}

pub fn register_shards() {
  register_shard::<InspectionShard>();
  register_shard::<SettingsShard>();
}
