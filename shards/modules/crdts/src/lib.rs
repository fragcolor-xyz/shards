#![allow(non_upper_case_globals)]

use lazy_static::lazy_static;
use serde::{Deserialize, Deserializer, Serialize, Serializer};
use shards::{fourCharacterCode, shlog_error, shlog_trace, SHType_Bytes, SHType_Int16};

use std::cell::RefCell;
use std::rc::Rc;

use shards::core::register_shard;
use shards::shard::Shard;
use shards::types::{common_type, ANY_TYPES, BYTES_TYPES, FRAG_CC, NONE_TYPES};
use shards::types::{ClonedVar, Context, ExposedTypes, InstanceData, ParamVar, Type, Types, Var};

use crdts::mvreg::*;
use crdts::CmRDT;
use crdts::{map::*, CvRDT};

lazy_static! {
  static ref CRDT_TYPE: Type = Type::object(FRAG_CC, fourCharacterCode(*b"crdt"));
  static ref CRDT_TYPE_VEC: Vec<Type> = vec![*CRDT_TYPE];
  static ref CRDT_VAR_TYPE: Type = Type::context_variable(&CRDT_TYPE_VEC);
  static ref CRDT_TYPES: Vec<Type> = vec![*CRDT_TYPE];
}

#[derive(Default, Hash, PartialEq, Eq, PartialOrd, Ord, Clone, Debug)]
struct CRDTVar(Rc<ClonedVar>);

impl Serialize for CRDTVar {
  fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
  where
    S: Serializer,
  {
    self.0.serialize(serializer)
  }
}

impl<'de> Deserialize<'de> for CRDTVar {
  fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
  where
    D: Deserializer<'de>,
  {
    let var = ClonedVar::deserialize(deserializer)?;
    Ok(Self(Rc::new(var)))
  }
}

impl From<&Var> for CRDTVar {
  fn from(var: &Var) -> Self {
    Self(Rc::new(var.into()))
  }
}

type CRDT = Map<CRDTVar, MVReg<CRDTVar, u128>, u128>;
type CRDTOp = crdts::map::Op<CRDTVar, MVReg<CRDTVar, u128>, u128>;

struct Document {
  crdt: CRDT,
  client_id: u128,
}

// CRDT.Open
#[derive(shards::shard)]
#[shard_info(
  "CRDT.Open",
  "Opens an empty CRDT document, returning the CRDT instance."
)]
struct CRDTOpenShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("ClientID", "The local client id.", [common_type::int16, common_type::int16_var, common_type::bytes, common_type::bytes_var])]
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
    self.crdt = Rc::new(RefCell::new(None));
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, _input: &Var) -> Result<Var, &str> {
    if self.output.is_none() {
      let client_id = self.client_id.get();
      let client_id = match client_id.valueType {
        SHType_Int16 => client_id.try_into().unwrap(), // qed: we know it's int16
        SHType_Bytes => {
          let bytes: &[u8] = client_id.try_into().unwrap(); // qed: we know it's bytes
                                                            // ensure it's 16 bytes and convert to u128
          if bytes.len() != 16 {
            return Err("ClientID must be 16 bytes.");
          }
          let mut client_id = [0u8; 16];
          client_id.copy_from_slice(bytes);
          u128::from_be_bytes(client_id)
        }
        _ => return Err("ClientID must be an int16 or 16 bytes."),
      };

      if client_id == 0 {
        return Err("ClientID cannot be 0.");
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

// CRDT.Load, loads a serialized CRDT into a CRDT instance
#[derive(shards::shard)]
#[shard_info("CRDT.Load", "Loads a serialized CRDT into a CRDT instance.")]
struct CRDTLoadShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("CRDT", "The CRDT instance to load into.", [*CRDT_VAR_TYPE])]
  crdt_param: ParamVar,

  crdt: Option<Rc<RefCell<Option<Document>>>>,
}

impl Default for CRDTLoadShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      crdt_param: ParamVar::default(),
      crdt: None,
    }
  }
}

#[shards::shard_impl]
impl Shard for CRDTLoadShard {
  fn input_types(&mut self) -> &Types {
    &BYTES_TYPES
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
    self.crdt = None;
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
    let slice: &[u8] = input.try_into().unwrap(); // qed: we know it's bytes
    let mut binding = self.crdt.as_ref().unwrap().borrow_mut();
    let doc = binding.as_mut().unwrap();
    let crdt = &mut doc.crdt;
    *crdt = bincode::deserialize(slice).unwrap();
    Ok(Var::default())
  }
}

// CRDT.Save serializes a CRDT instance
#[derive(shards::shard)]
#[shard_info("CRDT.Save", "Serializes a CRDT instance.")]
struct CRDTSaveShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("CRDT", "The CRDT instance to save.", [*CRDT_VAR_TYPE])]
  crdt_param: ParamVar,

  crdt: Option<Rc<RefCell<Option<Document>>>>,
  output: Vec<u8>,
}

impl Default for CRDTSaveShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      crdt_param: ParamVar::default(),
      crdt: None,
      output: Vec::new(),
    }
  }
}

#[shards::shard_impl]
impl Shard for CRDTSaveShard {
  fn input_types(&mut self) -> &Types {
    &NONE_TYPES
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
    self.crdt = None;
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, _input: &Var) -> Result<Var, &str> {
    if self.crdt.is_none() {
      let crdt = self.crdt_param.get();
      let crdt = Var::from_object_as_clone(crdt, &CRDT_TYPE)?;
      self.crdt = Some(crdt);
    }
    let mut binding = self.crdt.as_ref().unwrap().borrow_mut();
    let doc = binding.as_mut().unwrap();
    let crdt = &doc.crdt;
    shlog_trace!("CRDT.Save: {:?}", crdt);
    self.output = bincode::serialize(crdt).unwrap();
    Ok(self.output.as_slice().into())
  }
}

// CRDT.Set
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

  key_cache: Option<CRDTVar>,
}

impl Default for CRDTSetShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      crdt_param: ParamVar::default(),
      key: ParamVar::default(),
      crdt: None,
      output: Vec::new(),
      key_cache: None,
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
    self.crdt = None;
    self.key_cache = None;
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Var, &str> {
    // initialize the CRDT if it's not already
    if self.crdt.is_none() {
      let crdt = self.crdt_param.get();
      let crdt = Var::from_object_as_clone(crdt, &CRDT_TYPE)?;
      self.crdt = Some(crdt);
    }

    let mut binding = self.crdt.as_ref().unwrap().borrow_mut();
    let doc = binding.as_mut().unwrap();
    let crdt = &mut doc.crdt;
    let key = self.key.get();

    if let Some(key_cache) = &self.key_cache {
      if key_cache.0 .0 != *key {
        self.key_cache = Some(CRDTVar(Rc::new(key.into())));
      }
    } else {
      self.key_cache = Some(CRDTVar(Rc::new(key.into())));
    }

    let key_clone = self.key_cache.as_ref().unwrap().clone();

    // create thew new operation
    let add_ctx = crdt.read_ctx().derive_add_ctx(doc.client_id);
    let op = crdt.update(key_clone, add_ctx, |reg, ctx| reg.write(input.into(), ctx));
    shlog_trace!("CRDT.Set: {:?}, crdt: {:?}", op, crdt);

    // Validation not working.. will fail on second apply
    crdt.validate_op(&op).map_err(|e| {
      shlog_error!("CRDT.Delete error: {:?}", e);
      "Operation is invalid."
    })?;

    // encode the operation
    self.output = bincode::serialize(&op).unwrap();

    // apply the operation
    crdt.apply(op);

    // just return the slice reference, we hold those bytes for this activation lifetime
    Ok(self.output.as_slice().into())
  }
}

// CRDT.Get retrieves the value at the specified key in the CRDT instance
#[derive(shards::shard)]
#[shard_info(
  "CRDT.Get",
  "Retrieves the value at the specified key in the CRDT instance."
)]
struct CRDTGetShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("CRDT", "The CRDT instance to read from.", [*CRDT_VAR_TYPE])]
  crdt_param: ParamVar,

  #[shard_param("Key", "The key to read from the CRDT instance.", [common_type::any])]
  key: ParamVar,

  crdt: Option<Rc<RefCell<Option<Document>>>>,

  key_cache: Option<CRDTVar>,
}

impl Default for CRDTGetShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      crdt_param: ParamVar::default(),
      key: ParamVar::default(),
      crdt: None,
      key_cache: None,
    }
  }
}

#[shards::shard_impl]
impl Shard for CRDTGetShard {
  fn input_types(&mut self) -> &Types {
    &NONE_TYPES
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
    self.crdt = None;
    self.key_cache = None;
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, _input: &Var) -> Result<Var, &str> {
    // initialize the CRDT if it's not already
    if self.crdt.is_none() {
      let crdt = self.crdt_param.get();
      let crdt = Var::from_object_as_clone(crdt, &CRDT_TYPE)?;
      self.crdt = Some(crdt);
    }

    let binding = self.crdt.as_ref().unwrap().borrow();
    let doc = binding.as_ref().unwrap();
    let crdt = &doc.crdt;
    let key = self.key.get();

    if let Some(key_cache) = &self.key_cache {
      if key_cache.0 .0 != *key {
        self.key_cache = Some(CRDTVar(Rc::new(key.into())));
      }
    } else {
      self.key_cache = Some(CRDTVar(Rc::new(key.into())));
    }

    let value = crdt.get(self.key_cache.as_ref().unwrap());
    let value = value.val;
    if let Some(value) = value {
      let value = value.read();
      let value = value.val.first().unwrap();
      Ok(value.0 .0)
    } else {
      Ok(Var::default())
    }
  }
}

// CRDT.Delete deletes the value at the specified key in the CRDT instance
#[derive(shards::shard)]
#[shard_info(
  "CRDT.Delete",
  "Deletes the value at the specified key in the CRDT instance."
)]
struct CRDTDeleteShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("CRDT", "The CRDT instance to delete from.", [*CRDT_VAR_TYPE])]
  crdt_param: ParamVar,

  #[shard_param("Key", "The key to delete from the CRDT instance.", [common_type::any])]
  key: ParamVar,

  crdt: Option<Rc<RefCell<Option<Document>>>>,
  output: Vec<u8>,

  key_cache: Option<CRDTVar>,
}

impl Default for CRDTDeleteShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      crdt_param: ParamVar::default(),
      key: ParamVar::default(),
      crdt: None,
      output: Vec::new(),
      key_cache: None,
    }
  }
}

#[shards::shard_impl]
impl Shard for CRDTDeleteShard {
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
    self.crdt = None;
    self.key_cache = None;
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, _input: &Var) -> Result<Var, &str> {
    // initialize the CRDT if it's not already
    if self.crdt.is_none() {
      let crdt = self.crdt_param.get();
      let crdt = Var::from_object_as_clone(crdt, &CRDT_TYPE)?;
      self.crdt = Some(crdt);
    }

    let mut binding = self.crdt.as_ref().unwrap().borrow_mut();
    let doc = binding.as_mut().unwrap();
    let crdt = &mut doc.crdt;
    let key = self.key.get();

    if let Some(key_cache) = &self.key_cache {
      if key_cache.0 .0 != *key {
        self.key_cache = Some(CRDTVar(Rc::new(key.into())));
      }
    } else {
      self.key_cache = Some(CRDTVar(Rc::new(key.into())));
    }

    let key_clone = self.key_cache.as_ref().unwrap().clone();

    // create thew new operation
    let op = crdt.rm(key_clone, crdt.read_ctx().derive_rm_ctx());
    shlog_trace!("CRDT.Delete: {:?}", op);

    // Validation not working.. will fail on second apply
    crdt.validate_op(&op).map_err(|e| {
      shlog_error!("CRDT.Delete error: {:?}", e);
      "Operation is invalid."
    })?;

    // encode the operation
    self.output = bincode::serialize(&op).unwrap();

    // apply the operation
    crdt.apply(op);

    // just return the slice reference, we hold those bytes for this activation lifetime
    Ok(self.output.as_slice().into())
  }
}

// CRDT.Merge merges two CRDT instances
#[derive(shards::shard)]
#[shard_info("CRDT.Merge", "Merges two CRDT instances.")]
struct CRDTMergeShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("Into", "The CRDT instance to merge into.", [*CRDT_VAR_TYPE])]
  crdt1_param: ParamVar,

  #[shard_param("Other", "The other CRDT instance to merge.", [*CRDT_VAR_TYPE])]
  crdt2_param: ParamVar,

  crdt1: Option<Rc<RefCell<Option<Document>>>>,
  crdt2: Option<Rc<RefCell<Option<Document>>>>,
}

impl Default for CRDTMergeShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      crdt1_param: ParamVar::default(),
      crdt2_param: ParamVar::default(),
      crdt1: None,
      crdt2: None,
    }
  }
}

#[shards::shard_impl]
impl Shard for CRDTMergeShard {
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
    self.crdt1 = None;
    self.crdt2 = None;
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Var, &str> {
    if self.crdt1.is_none() {
      let crdt = self.crdt1_param.get();
      let crdt = Var::from_object_as_clone(crdt, &CRDT_TYPE)?;
      self.crdt1 = Some(crdt);
    }
    if self.crdt2.is_none() {
      let crdt = self.crdt2_param.get();
      let crdt = Var::from_object_as_clone(crdt, &CRDT_TYPE)?;
      self.crdt2 = Some(crdt);
    }
    let mut binding1 = self.crdt1.as_ref().unwrap().borrow_mut();
    let doc1 = binding1.as_mut().unwrap();
    let crdt1 = &mut doc1.crdt;
    let mut binding2 = self.crdt2.as_ref().unwrap().borrow_mut();
    let doc2 = binding2.as_mut().unwrap();
    let crdt2 = &mut doc2.crdt;
    let crdt2 = crdt2.clone();

    crdt1.validate_merge(&crdt2).map_err(|e| {
      shlog_error!("CRDT.Merge error: {:?}", e);
      "CRDTs cannot be merged."
    })?;

    crdt1.merge(crdt2);

    Ok(*input)
  }
}

// CRDT.Apply applies an operation to a CRDT instance
#[derive(shards::shard)]
#[shard_info("CRDT.Apply", "Applies an operation to a CRDT instance.")]
struct CRDTApplyShard {
  #[shard_required]
  required: ExposedTypes,

  #[shard_param("CRDT", "The CRDT instance to apply the operation to.", [*CRDT_VAR_TYPE])]
  crdt_param: ParamVar,

  crdt: Option<Rc<RefCell<Option<Document>>>>,

  deferred: Vec<CRDTOp>,
}

impl Default for CRDTApplyShard {
  fn default() -> Self {
    Self {
      required: ExposedTypes::new(),
      crdt_param: ParamVar::default(),
      crdt: None,
      deferred: Vec::new(),
    }
  }
}

#[shards::shard_impl]
impl Shard for CRDTApplyShard {
  fn input_types(&mut self) -> &Types {
    &BYTES_TYPES
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
    self.deferred.clear();
    self.crdt = None;
    Ok(())
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    Ok(self.output_types()[0])
  }

  fn activate(&mut self, _context: &Context, input: &Var) -> Result<Var, &str> {
    // initialize the CRDT if it's not already
    if self.crdt.is_none() {
      let crdt = self.crdt_param.get();
      let crdt = Var::from_object_as_clone(crdt, &CRDT_TYPE)?;
      self.crdt = Some(crdt);
    }

    let mut binding = self.crdt.as_ref().unwrap().borrow_mut();
    let doc = binding.as_mut().unwrap();
    let crdt = &mut doc.crdt;
    let slice: &[u8] = input.try_into().unwrap(); // qed: we know it's bytes

    // decode the operation
    let op: CRDTOp = bincode::deserialize(slice).unwrap();
    shlog_trace!("CRDT.Apply: {:?}", op);

    // Validation not working.. will fail on second apply
    let res = crdt.validate_op(&op);

    match res {
      Err(CmRDTValidation::SourceOrder(_)) => {
        self.deferred.push(op);
        return Ok(*input);
      }
      Err(e) => {
        shlog_error!("CRDT.Apply error: {:?}", e);
        return Err("Operation is invalid.");
      }
      _ => {}
    }

    // apply the operation
    crdt.apply(op);

    // brute force for now, try apply deferred operations in a loop, in whatever order
    // todo - implement a proper deferred operation queue
    // iterate by index in reverse order to remove applied operations
    for i in (0..self.deferred.len()).rev() {
      let op = &self.deferred[i];
      let res = crdt.validate_op(&op);
      match res {
        Err(CmRDTValidation::SourceOrder(_)) => continue,
        Err(e) => {
          shlog_error!("CRDT.Apply error: {:?}", e);
          return Err("Operation is invalid.");
        }
        _ => {}
      }
      let op = self.deferred.remove(i);
      crdt.apply(op);
    }

    Ok(*input)
  }
}

#[no_mangle]
pub extern "C" fn shardsRegister_crdts_crdts(core: *mut shards::shardsc::SHCore) {
  unsafe {
    shards::core::Core = core;
  }

  register_shard::<CRDTOpenShard>();

  register_shard::<CRDTLoadShard>();
  register_shard::<CRDTSaveShard>();

  register_shard::<CRDTSetShard>();
  register_shard::<CRDTGetShard>();
  register_shard::<CRDTDeleteShard>();

  register_shard::<CRDTMergeShard>();
  register_shard::<CRDTApplyShard>();
}
