use lazy_static::lazy_static;
use shards::{fourCharacterCode, SHObjectTypeInfo};

use std::cell::RefCell;
use std::rc::Rc;

use shards::core::{register_enum, register_shard};
use shards::shard::Shard;
use shards::types::{common_type, ANY_TYPES, FRAG_CC, NONE_TYPES};
use shards::types::{ClonedVar, Context, ExposedTypes, InstanceData, ParamVar, Type, Types, Var};

use crdts::map::*;
use crdts::mvreg::*;
use crdts::CmRDT;

lazy_static! {
  static ref CRDT_TYPE: Type = Type::object(FRAG_CC, fourCharacterCode(*b"crdt"));
  static ref CRDT_TYPE_VEC: Vec<Type> = vec![*CRDT_TYPE];
  static ref CRDT_VAR_TYPE: Type = Type::context_variable(&CRDT_TYPE_VEC);
  static ref CRDT_TYPES: Vec<Type> = vec![*CRDT_TYPE];
}

type CRDT = Map<ClonedVar, MVReg<ClonedVar, ClonedVar>, ClonedVar>;

#[derive(shards::shard)]
#[shard_info("CRDT.Open", "Opens an existing CRDT document or creates a new one from a specified file, returning the CRDT instance.")]
struct CRDTOpenShard {
  #[shard_required]
  required: ExposedTypes,

  crdt: Rc<RefCell<Option<CRDT>>>,
  output: Var,
}

impl Default for CRDTOpenShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      crdt: Rc::new(RefCell::new(None)),
      output: Var::default(),
    }
  }
}

#[shards::shard_impl]
impl Shard for CRDTOpenShard {
  fn input_types(&mut self) -> &Types {
    &NONE_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &CRDT_TYPES
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;

    let _ = self.crdt.replace(Some(Map::new()));
    self.output = Var::new_object(&self.crdt, &CRDT_TYPE);

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;

    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, _input: &Var) -> Result<Var, &str> {
    Ok(self.output)
  }
}

#[derive(shards::shard)]
#[shard_info(
  "CRDT.Set",
  "Updates or adds a value in the CRDT instance at the specified key, returning the updated CRDT."
)]
struct CRDTSetShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("CRDT", "The CRDT instance to edit.", [*CRDT_VAR_TYPE])]
  crdt_param: ParamVar,

  #[shard_param("Key", "The key to update in the CRDT instance.", [common_type::any])]
  key: ParamVar,

  crdt: Rc<RefCell<Option<CRDT>>>,
}

impl Default for CRDTSetShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      crdt_param: ParamVar::default(),
      key: ParamVar::default(),
      crdt: Rc::new(RefCell::new(None)),
    }
  }
}

#[shards::shard_impl]
impl Shard for CRDTSetShard {
  fn input_types(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn warmup(&mut self, ctx: &Context) -> Result<(), &str> {
    self.warmup_helper(ctx)?;

    let crdt = self.crdt_param.get();
    self.crdt = Var::from_object_as_clone(crdt, &CRDT_TYPE)?;

    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;

    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Var, &str> {
    let mut binding = self.crdt.borrow_mut();
    let crdt = binding.as_mut().unwrap();
    let key = self.key.get();
    // create thew new operation
    let op = crdt.update(
      key,
      crdt.read_ctx().derive_add_ctx(Var::default().into()),
      |reg, ctx| reg.write(input.into(), ctx),
    );
    // apply the operation
    crdt.apply(op);
    Ok(*input)
  }
}

#[no_mangle]
pub extern "C" fn shardsRegister_crdts_crdts(core: *mut shards::shardsc::SHCore) {
  unsafe {
    shards::core::Core = core;
  }

  register_shard::<CRDTOpenShard>();
}
