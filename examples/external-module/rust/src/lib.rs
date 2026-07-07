use shards::core::register_shard;
use shards::shard::Shard;
use shards::types::{ClonedVar, Context, ExposedTypes, InstanceData, Type, Types, Var, STRING_TYPES};

#[derive(shards::shard)]
#[shard_info("Example.Reverse", "Reverses the input string.")]
struct ExampleReverse {
  #[shard_required]
  required: ExposedTypes,
  output: ClonedVar,
}

impl Default for ExampleReverse {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      output: ClonedVar::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for ExampleReverse {
  fn input_types(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &STRING_TYPES
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;
    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    self.output = ClonedVar::default();
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let text: &str = input.try_into()?;
    let reversed: String = text.chars().rev().collect();
    self.output = reversed.into();
    Ok(Some(self.output.0))
  }
}

fn register_shards() {
  register_shard::<ExampleReverse>();
}

/// Static path: called by the generated registry (registerModuleShards) when
/// this crate is built into the rust union. The name must match the module id
/// and REGISTER_SHARDS entry in CMakeLists.txt: shardsRegister_<module>_<id>.
#[no_mangle]
pub extern "C" fn shardsRegister_example_rust(core: *mut shards::shardsc::SHCore) {
  unsafe {
    shards::core::Core = core;
  }
  register_shards();
}

/// Dynamic path: self-registration at dlopen time when built as a standalone
/// plugin (`cargo build --features dylib`). shards::core::init() resolves the
/// core vtable through the exported shardsInterface symbol of the host.
#[cfg(feature = "dylib")]
#[ctor::ctor]
fn plugin_entry() {
  shards::core::init();
  register_shards();
}
