use egui::Sense;
use shards::shard::Shard;
use shards::types::{ClonedVar, SeqVar, ShardsVar, ANY_TYPES, SEQ_OF_SHARDS_TYPES};
use shards::types::{Context, ExposedTypes, InstanceData, ParamVar, Type, Types, Var};

use crate::{util, PARENTS_UI_NAME};

lazy_static! {
  static ref ITEM_TYPE: Type = Type::seq(&*SEQ_OF_SHARDS_TYPES);
  static ref ITEM_TYPES: Types = vec![*ITEM_TYPE];
}

#[derive(shards::shard)]
#[shard_info("UI.Overlay", "Overlays multiple UI's on top of each other.")]
pub struct OverlayShard {
  #[shard_required]
  required: ExposedTypes,
  #[shard_param(
    "Items",
    "A list of UI's to overlay on top of each other in order",
    ITEM_TYPES
  )]
  items: ClonedVar,
  #[shard_warmup]
  parents: ParamVar,
  calls: Vec<ShardsVar>,
}

impl Default for OverlayShard {
  fn default() -> Self {
    Self {
      parents: ParamVar::new_named(PARENTS_UI_NAME),
      required: ExposedTypes::new(),
      items: ClonedVar::default(),
      calls: Vec::new(),
    }
  }
}

#[shards::shard_impl]
impl Shard for OverlayShard {
  fn input_types(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;
    for c in &mut self.calls {
      c.warmup(ctx)?;
    }
    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    for c in &mut self.calls {
      c.cleanup(ctx);
    }
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;

    util::require_parents(&mut self.required);

    self.calls.clear();
    let seq: &SeqVar = (&self.items.0).as_seq()?;
    for item in seq {
      let mut sv = ShardsVar::default();
      sv.set_param(&item)?;
      sv.compose(data)?;
      // item.
      self.calls.push(sv);
    }

    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, _input: &Var) -> Result<Option<Var>, &str> {
    let ui = util::get_current_parent_opt(self.parents.get())?.ok_or("No parent UI")?;

    // let min_rect = ui.min_rect();
    let max_rect = ui.max_rect();
    let mut used_ui = ui.min_rect();
    for c in &mut self.calls {
      let mut child_ui = ui.child_ui(max_rect, ui.layout().clone(), None);

      let none_var = Var::default();
      util::activate_ui_contents(_context, &none_var, &mut child_ui, &mut self.parents, c)?;
      used_ui.extend_with(child_ui.min_rect().max);
    }

    ui.allocate_rect(used_ui, Sense::hover());

    Ok(None)
  }
}
