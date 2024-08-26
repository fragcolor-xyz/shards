#[macro_use]
extern crate lazy_static;

use shards::{
  core::{register_shard, Core},
  cstr,
  shard::Shard,
  shccstr,
  types::{ClonedVar, Context, OptionalString, Types, Var, INT_TYPES, NONE_TYPES, STRING_TYPES},
  SHCore,
};

#[derive(shards::shard)]
#[shard_info("Random.Name", "Generate a random name (Petname)")]
pub struct RandomName {
  #[shard_param("Words", "How many words to generate and concatenate", INT_TYPES)]
  pub words_count: ClonedVar,
  #[shard_param(
    "Separator",
    "A separator character to use between generated words",
    STRING_TYPES
  )]
  pub separator: ClonedVar,
  output: ClonedVar,
}

impl Default for RandomName {
  fn default() -> Self {
    Self {
      words_count: 2.into(),
      separator: Var::ephemeral_string("-").into(),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for RandomName {
  fn input_types(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn activate(&mut self, _: &Context, _: &Var) -> Result<Option<Var>, &str> {
    let words_count: i64 = self.words_count.0.as_ref().try_into()?;
    let separator: &str = self.separator.0.as_ref().try_into()?;
    let mut rng = rand::thread_rng();
    let pname = petname::Petnames::default().generate(&mut rng, words_count as u8, separator);
    self.output = pname.into();
    Ok(Some(self.output.0))
  }
}

#[no_mangle]
pub extern "C" fn shardsRegister_random_rust(core: *mut SHCore) {
  unsafe {
    Core = core;
  }
  register_shard::<RandomName>();
}
