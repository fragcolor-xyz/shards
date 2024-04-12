use lazy_static::lazy_static;
use shards::{fourCharacterCode, SHObjectTypeInfo};

use std::cell::RefCell;
use std::rc::Rc;

use shards::core::{register_enum, register_shard};
use shards::shard::Shard;
use shards::types::{common_type, ANY_TYPES, BYTES_TYPES, FRAG_CC, NONE_TYPES};
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

type CRDT = Map<ClonedVar, MVReg<ClonedVar, u128>, u128>;

struct Document {
  crdt: CRDT,
  client_id: u128,
}

#[derive(shards::shard)]
#[shard_info("CRDT.Open", "Opens an existing CRDT document or creates a new one from a specified file, returning the CRDT instance.")]
struct CRDTOpenShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("ClientID", "The local client id.", [common_type::int16, common_type::int16_var])]
  client_id: ParamVar,

  crdt: Rc<RefCell<Option<Document>>>,
  output: Var,
}

impl Default for CRDTOpenShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      client_id: ParamVar::new(0.into()),
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

    self.output = Var::default();

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
    if self.output.is_none() {
      let client_id = self.client_id.get();
      let client_id: u128 = client_id.into();
      if client_id == 0 {
        return Err("ClientID must be greater than 0.");
      }
      let _ = self.crdt.replace(Some(Document {
        crdt: Map::new(),
        client_id,
      }));
      self.output = Var::new_object(&self.crdt, &CRDT_TYPE);
    }
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

  crdt: Option<Rc<RefCell<Option<Document>>>>,
  output: Vec<u8>,
}

impl Default for CRDTSetShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      crdt_param: ParamVar::default(),
      key: ParamVar::default(),
      crdt: None,
      output: Vec::new(),
    }
  }
}

#[shards::shard_impl]
impl Shard for CRDTSetShard {
  fn input_types(&mut self) -> &Types {
    &ANY_TYPES
  }

  fn output_types(&mut self) -> &Types {
    &BYTES_TYPES
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
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Var, &str> {
    if self.crdt.is_none() {
      let crdt = self.crdt_param.get();
      let crdt = Var::from_object_as_clone(crdt, &CRDT_TYPE)?;
      self.crdt = Some(crdt);
    }
    let mut binding = self.crdt.as_ref().unwrap().borrow_mut();
    let doc = binding.as_mut().unwrap();
    let crdt = &mut doc.crdt;
    let key = self.key.get();
    // create thew new operation
    let op = crdt.update(
      key,
      crdt.read_ctx().derive_add_ctx(doc.client_id),
      |reg, ctx| reg.write(input.into(), ctx),
    );
    // encode the operation
    self.output = bincode::serialize(&op).unwrap();
    // apply the operation
    crdt.apply(op);
    // just return the slice reference, we hold those bytes for this activation lifetime
    Ok(self.output.as_slice().into())
  }
}

#[no_mangle]
pub extern "C" fn shardsRegister_crdts_crdts(core: *mut shards::shardsc::SHCore) {
  unsafe {
    shards::core::Core = core;
  }

  register_shard::<CRDTOpenShard>();
  register_shard::<CRDTSetShard>();
}
