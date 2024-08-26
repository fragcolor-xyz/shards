use super::TextWrap;
use crate::util;
use crate::widgets::text_util;
use crate::widgets::TEXTWRAP_TYPE;
use crate::ANY_TABLE_OR_NONE_SLICE;
use crate::PARENTS_UI_NAME;
use egui::TextWrapMode;
use shards::shard::Shard;
use shards::types::common_type;
use shards::types::ClonedVar;
use shards::types::Context;
use shards::types::ExposedTypes;
use shards::types::InstanceData;
use shards::types::ParamVar;
use shards::types::Type;
use shards::types::Types;
use shards::types::Var;
use shards::types::STRING_TYPES;

#[derive(shards::shard)]
#[shard_info("UI.Label", "Static text.")]
pub(crate) struct Label {
  #[shard_required]
  requiring: ExposedTypes,

  #[shard_warmup]
  parents: ParamVar,

  #[shard_param("Wrap", "The text wrapping mode.", [*TEXTWRAP_TYPE, common_type::bool])]
  wrap: ClonedVar,

  #[shard_param("Style", "The text style.", ANY_TABLE_OR_NONE_SLICE)]
  style: ClonedVar,
}

impl Default for Label {
  fn default() -> Self {
    Self {
      requiring: ExposedTypes::new(),
      parents: ParamVar::new_named(PARENTS_UI_NAME),
      wrap: ClonedVar(TextWrap::Extend.into()),
      style: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for Label {
  fn input_types(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.warmup_helper(context)?;
    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    util::require_parents(&mut self.requiring);
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    if let Some(ui) = util::get_current_parent_opt(self.parents.get())? {
      let text: &str = input.try_into()?;
      let mut text = egui::RichText::new(text);

      let style = &self.style.0;
      if !style.is_none() {
        text = text_util::get_styled_text(text, &style.try_into()?)?;
      }

      let mut label = egui::Label::new(text);

      let wrap = &self.wrap.0;
      if wrap.is_bool() {
        let wrap: bool = wrap.try_into()?;
        if wrap {
          label = label.wrap_mode(TextWrapMode::Wrap);
        } else {
          label = label.wrap_mode(TextWrapMode::Extend);
        }
      } else {
        let wrap: TextWrap = wrap.try_into()?;
        let wrap = match wrap {
          TextWrap::Truncate => TextWrapMode::Truncate,
          TextWrap::Wrap => TextWrapMode::Wrap,
          TextWrap::Extend => TextWrapMode::Extend,
        };
        label = label.wrap_mode(wrap);
      }

      ui.add(label);

      Ok(None)
    } else {
      Err("No UI parent")
    }
  }
}
