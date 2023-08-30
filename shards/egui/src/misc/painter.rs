use crate::util;
use shards::core::register_shard;
use shards::types::{
  Context, InstanceData, OptionalString, ParamVar, Type, Types, Var, NONE_TYPES,
};

use crate::shards::shard;
use crate::shards::shard::Shard;
use crate::HELP_VALUE_IGNORED;

shard! {
  struct CircleShard("UI.PaintCircle", "Paints a circle") {
    parents: ParamVar,
  }

  impl Shard for CircleShard {
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
      self.parents.warmup(context);
      Ok(())
    }
    fn cleanup(&mut self) -> Result<(), &str> {
      self.cleanup_helper()?;
      self.parents.cleanup();
      Ok(())
    }
    fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
      self.compose_helper(data)?;
      util::require_parents(&mut self.required);
      Ok(NONE_TYPES[0])
    }
    fn activate(&mut self, context: &Context, input: &Var) -> Result<Var, &str> {
      util::get_current_parent(self.parents.get())?;
      Ok(Var::default())
    }
  }
}

impl Default for CircleShard {
  fn default() -> Self {
    Self {
      parents: shards::types::ParamVar::new_named(crate::PARENTS_UI_NAME),
      required: Vec::new(),
    }
  }
}

pub fn register_shards() {
  register_shard::<CircleShard>();
}
