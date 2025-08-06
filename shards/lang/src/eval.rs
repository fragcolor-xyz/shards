use crate::ast::*;
use crate::custom_state::CustomStateContainer;
use crate::read;
use crate::read::AST_TYPE;
use crate::ParamHelper;
use crate::RcStrWrapper;
use crate::ShardsExtension;

use core::convert::TryInto;
use std::borrow::Cow;

use nanoid::nanoid;
use shards::fourCharacterCode;
use shards::ref_counted_object_type_impl;
use shards::shard;
use shards::shard::Shard;
use shards::shard_impl;
use shards::types::find_object_type_vendor_type_pair;
use shards::types::ExposedTypes;
use shards::types::InstanceData;
use shards::types::SeqVar;
use shards::types::BOOL_TYPES_SLICE;
use shards::types::FRAG_CC;
use shards::types::STRINGS_OR_NONE_SLICE;
use shards::SHType_Trait;

use shards::shlog_error;
use shards::types::common_type;
use shards::types::AutoSeqVar;
use shards::types::AutoShardRef;
use shards::types::AutoTableVar;
use shards::types::Context;
use shards::types::MeshVar;
use shards::types::ParamVar;
use shards::types::ANY_TABLE_VAR_NONE_SLICE;
use shards::types::STRING_VAR_OR_NONE_SLICE;

use shards::types::Type;
use shards::types::Types;

use shards::shlog_trace;

use shards::types::WIRE_TYPES;
use shards::SHType_Object;
use shards::SHType_Type;
use std::cell::RefCell;
use std::sync::atomic::AtomicBool;
use std::sync::Arc;

use std::collections::HashMap;
use std::collections::HashSet;

use shards::core::findEnumId;
use shards::core::findEnumInfo;
use shards::types::ClonedVar;
use shards::types::Mesh;
use shards::SHType_Enum;
use shards::SHType_String;

use shards::types::{ShardRef, Var, Wire};

use shards::{SHType_ContextVar, SHType_ShardRef};
use std::ffi::CStr;

use shards::util::from_raw_parts_allow_null;

const MIN_STACK_SIZE: i64 = shards::core::MIN_STACK_SIZE as i64;
pub(crate) const EVAL_STACK_SIZE: usize = 2 * 1024 * 1024;

pub fn new_cancellation_token() -> Arc<AtomicBool> {
  Arc::new(AtomicBool::new(false))
}

#[derive(Clone)]
struct ShardsGroupSource {
  args: *const Vec<Value>,
  shards: *const Sequence,
}

#[derive(Clone)]
struct ShardsGroupGenerated {
  args: Vec<Value>,
  shards: Sequence,
}

#[derive(Clone)]
enum ShardsGroup {
  Source(ShardsGroupSource),
  Generated(ShardsGroupGenerated),
}

impl From<ShardsGroupSource> for ShardsGroup {
  fn from(source: ShardsGroupSource) -> Self {
    ShardsGroup::Source(source)
  }
}

impl From<ShardsGroupGenerated> for ShardsGroup {
  fn from(generated: ShardsGroupGenerated) -> Self {
    ShardsGroup::Generated(generated)
  }
}

#[repr(C)]
#[derive(Clone, Copy)]
pub struct Setting {
  disallow_unsafe: bool,
  disallow_custom_stack_sizes: bool,
  disallow_impure_wires: bool,
}

#[derive(Clone)]
enum Definition {
  ValueSource(*const Value),
  ValueGenerated(Value),
  Constant(SVar),
}

enum WireParams {
  Source(*const Vec<Param>),
  Generated(Vec<Param>),
}

#[derive(Clone, Copy)]
enum ContextType {
  // Parsing directly from source ast, meaning all references will stay valid during evaluation
  Source,
  // Parsing from a generated AST or source code, meaning all references will be invalidated after exiting some scope
  Generated,
}

pub struct EvalEnv {
  pub program: Option<*const Program>,
  pub parent: Option<*const EvalEnv>,

  namespace: RcStrWrapper,
  full_namespace: RcStrWrapper,
  qualified_cache: HashMap<Identifier, RcStrWrapper>,

  shards: Vec<AutoShardRef>,

  deferred_wires: HashMap<Identifier, (Wire, WireParams, LineInfo)>,
  finalized_wires: HashMap<Identifier, ClonedVar>,

  shards_groups: HashMap<Identifier, ShardsGroup>,
  macro_groups: HashMap<Identifier, ShardsGroup>,
  definitions: HashMap<Identifier, Definition>,

  // used during @template evaluations, to replace [x y z] arguments
  replacements: HashMap<RcStrWrapper, *const Value>,

  traits: HashMap<Identifier, ClonedVar>,

  // used during @template evaluation
  suffix: Option<RcStrWrapper>,
  suffix_assigned: HashMap<RcStrWrapper, RcStrWrapper>, // maps var names to their suffix

  // Shards and functions that are forbidden to be used
  pub forbidden_funcs: HashSet<Identifier>,

  // Shards that need rewriting, e.g. upgrading to new versions
  pub rewrite_funcs: HashMap<Identifier, Arc<dyn RewriteFunction>>,

  pub settings: Vec<Setting>,

  meshes: HashMap<Identifier, MeshVar>,

  extensions: HashMap<Identifier, Arc<dyn ShardsExtension>>,
  default_line_info: Option<LineInfo>,

  complexity: u64,
  context_type: ContextType,
}

impl Drop for EvalEnv {
  fn drop(&mut self) {
    self.shards.clear();
    self.finalized_wires.clear();
    self.deferred_wires.clear();
    self.definitions.clear();
    // Explicitly keep meshes alive as long as possible
    self.meshes.clear();
  }
}

impl EvalEnv {
  pub fn new(
    namespace: Option<RcStrWrapper>,
    parent: Option<*const EvalEnv>,
    program: Option<*const Program>,
  ) -> Self {
    let mut env = EvalEnv {
      program,
      parent: None,
      namespace: RcStrWrapper::from_const(""),
      full_namespace: RcStrWrapper::from_const(""),
      qualified_cache: HashMap::new(),
      shards: Vec::new(),
      deferred_wires: HashMap::new(),
      finalized_wires: HashMap::new(),
      shards_groups: HashMap::new(),
      macro_groups: HashMap::new(),
      definitions: HashMap::new(),
      replacements: HashMap::new(),
      suffix: None,
      suffix_assigned: HashMap::new(),
      forbidden_funcs: HashSet::new(),
      rewrite_funcs: HashMap::new(),
      settings: Vec::new(),
      meshes: HashMap::new(),
      extensions: HashMap::new(),
      traits: HashMap::new(),
      complexity: 0,
      context_type: ContextType::Source,
      default_line_info: None,
    };

    if let Some(parent) = parent {
      // resolve namespaces
      let parent = unsafe { &*parent };
      env.full_namespace = parent.full_namespace.clone();
      env.settings = parent.settings.clone();
      env.complexity = parent.complexity;
      env.context_type = parent.context_type;
    }
    env.parent = parent;

    if let Some(namespace) = namespace {
      env.namespace = namespace.clone();
      if !env.full_namespace.is_empty() {
        let s = format!("{}/{}", env.full_namespace, namespace);
        env.full_namespace = RcStrWrapper::from(s);
      } else {
        env.full_namespace = namespace;
      }
    }

    env
  }

  fn from_captured(env: &CapturedEvalContext) -> Self {
    let mut result = EvalEnv {
      program: None,
      parent: None,
      namespace: env.namespace.clone(),
      full_namespace: env.full_namespace.clone(),
      qualified_cache: HashMap::new(),
      shards: Vec::new(),
      deferred_wires: HashMap::new(),
      finalized_wires: HashMap::new(),
      shards_groups: HashMap::new(),
      macro_groups: HashMap::new(),
      definitions: HashMap::new(),
      replacements: HashMap::new(),
      suffix: env.suffix.clone(),
      suffix_assigned: env.suffix_assigned.clone(),
      forbidden_funcs: env.forbidden_funcs.clone(),
      rewrite_funcs: env.rewrite_funcs.clone(),
      settings: env.settings.clone(),
      meshes: env.meshes.clone(),
      extensions: env.extensions.clone(),
      traits: HashMap::new(),
      complexity: 0,
      context_type: ContextType::Source,
      default_line_info: None,
    };

    // Convert ClonedDefinition to Definition
    for (name, def) in &env.definitions {
      result.definitions.insert(name.clone(), def.clone());
    }

    // Convert ClonedShardsGroup to ShardsGroup for macro_groups
    for (name, group) in &env.macro_groups {
      result.macro_groups.insert(name.clone(), group.clone());
    }

    // Convert ClonedShardsGroup to ShardsGroup for shards_groups
    for (name, group) in &env.shards_groups {
      result.shards_groups.insert(name.clone(), group.clone());
    }

    // Convert replacements
    for (name, value) in &env.replacements {
      let boxed_value = Box::new(value.clone());
      let value_ptr = Box::into_raw(boxed_value);
      result.replacements.insert(name.clone(), value_ptr);
    }

    // Convert traits
    for (name, trait_var) in &env.traits {
      result.traits.insert(name.clone(), trait_var.clone());
    }

    // Convert finalized_wires
    for (name, wire_var) in &env.wires {
      result
        .finalized_wires
        .insert(name.clone(), wire_var.clone());
    }

    result
  }

  fn lookup<'a, R, F>(&self, f: F) -> Option<R>
  where
    F: Fn(&'a mut EvalEnv) -> Option<R>,
  {
    let mut current: Option<*const EvalEnv> = Some(self);
    while let Some(env1) = current {
      let env = unsafe { &mut *(env1 as *mut EvalEnv) };
      let r = f(env);
      if r.is_some() {
        return r;
      }
      current = unsafe { &*env1 }.parent;
    }
    None
  }

  fn find_mesh<'a>(&self, name: &'a Identifier) -> Option<&'a mut MeshVar> {
    self.lookup(|env| env.meshes.get_mut(name))
  }

  fn find_trait<'a>(&self, name: &'a Identifier) -> Option<Var> {
    self.lookup(|env| env.traits.get(name).map(|t| t.0.into()))
  }

  fn find_shards_group<'a>(&self, name: &'a Identifier) -> Option<&'a ShardsGroup> {
    self.lookup(|env| env.shards_groups.get(name))
  }

  fn find_macro_group<'a>(&self, name: &'a Identifier) -> Option<&'a ShardsGroup> {
    self.lookup(|env| env.macro_groups.get(name))
  }

  fn find_defined<'a>(&self, name: &'a Identifier) -> Option<&'a Definition> {
    self.lookup(|env| env.definitions.get(name))
  }

  fn get_program<'a>(&self) -> Option<&'a Program> {
    self.lookup(|env| env.program.map(|p| unsafe { &*p }))
  }

  fn is_forbidden_func<'a>(&self, name: &'a Identifier) -> bool {
    self
      .lookup(|env| {
        if env.forbidden_funcs.contains(name) {
          Some(true)
        } else {
          None
        }
      })
      .unwrap_or(false)
  }

  fn get_rewrite_func<'a>(&self, name: &'a Identifier) -> Option<&'a Arc<dyn RewriteFunction>> {
    self.lookup(|env| env.rewrite_funcs.get(name))
  }
  fn find_current_suffix<'a>(&self) -> Option<&'a RcStrWrapper> {
    self.lookup(|env| env.suffix.as_ref())
  }

  fn find_suffix<'a>(&self, name: &'a RcStrWrapper) -> Option<&'a RcStrWrapper> {
    self.lookup(|env| env.suffix_assigned.get(name))
  }

  fn find_replacement<'a>(&self, name: &'a Identifier) -> Option<&'a Value> {
    // Ignore explicitly qualified variables like ext/base-url
    if name.namespaces.len() > 0 {
      return None;
    }
    self.lookup(|env| {
      let name = &name.name;
      if let Some(replacement) = env.replacements.get(name) {
        let replacement = *replacement;
        let replacement = unsafe { &*replacement };
        Some(replacement)
      } else {
        None
      }
    })
  }

  fn find_wire<'a>(&self, name: &'a Identifier) -> Option<(Var, bool)> {
    self.lookup(|env| {
      if let Some(wire) = env.finalized_wires.get(name) {
        Some((wire.0.into(), true))
      } else if let Some(wire) = env.deferred_wires.get(name) {
        Some((wire.0 .0.into(), false))
      } else {
        None
      }
    })
  }

  fn find_extension<'a>(&self, name: &'a Identifier) -> Option<&'a Arc<dyn ShardsExtension>> {
    self.lookup(|env| env.extensions.get(name))
  }

  fn find_default_line_info(&self) -> Option<LineInfo> {
    self.lookup(|env| env.default_line_info.clone())
  }

  fn with_line_info<R, F>(&mut self, line_info: LineInfo, f: F) -> R
  where
    F: FnOnce(&mut Self) -> R,
  {
    let prev = self.default_line_info;
    self.default_line_info = Some(line_info);
    let result = f(self);
    self.default_line_info = prev;
    result
  }

  fn with_context_mut<R, F>(&mut self, context_type: ContextType, f: F) -> R
  where
    F: FnOnce(&mut Self) -> R,
  {
    let prev = self.context_type;
    self.context_type = context_type;
    let result = f(self);
    self.context_type = prev;
    result
  }
}

fn get_block_line_info(e: &EvalEnv, block: &Block) -> LineInfo {
  block
    .line_info
    .unwrap_or_else(|| e.find_default_line_info().unwrap_or_default())
}

impl ShardsGroup {
  fn without_pointers(&self) -> Self {
    match self {
      ShardsGroup::Source(source) => ShardsGroup::Generated(ShardsGroupGenerated {
        args: unsafe { &*source.args }.clone(),
        shards: unsafe { &*source.shards }.clone(),
      }),
      ShardsGroup::Generated(generated) => ShardsGroup::Generated(generated.clone()),
    }
  }
}

impl Definition {
  fn without_pointers(&self) -> Self {
    match self {
      Definition::ValueGenerated(v) => Definition::ValueGenerated(v.clone()),
      Definition::ValueSource(v) => Definition::ValueGenerated(unsafe { (**v).clone() }),
      Definition::Constant(v) => Definition::Constant(v.clone()),
    }
  }
}

// Captured EvalContext passed into shards
struct CapturedEvalContext {
  cached_definition_pointers: HashMap<Identifier, Definition>,
  cached_macro_group_pointers: HashMap<Identifier, ShardsGroup>,
  cached_shards_group_pointers: HashMap<Identifier, ShardsGroup>,

  definitions: HashMap<Identifier, Definition>,
  extensions: HashMap<Identifier, Arc<dyn ShardsExtension>>,
  forbidden_funcs: HashSet<Identifier>,
  full_namespace: RcStrWrapper,
  macro_groups: HashMap<Identifier, ShardsGroup>,
  shards_groups: HashMap<Identifier, ShardsGroup>,
  meshes: HashMap<Identifier, MeshVar>,
  namespace: RcStrWrapper,
  replacements: HashMap<RcStrWrapper, Value>,
  rewrite_funcs: HashMap<Identifier, Arc<dyn RewriteFunction>>,
  settings: Vec<Setting>,
  suffix: Option<RcStrWrapper>,
  suffix_assigned: HashMap<RcStrWrapper, RcStrWrapper>,
  traits: HashMap<Identifier, ClonedVar>,
  wires: HashMap<Identifier, ClonedVar>,
}
ref_counted_object_type_impl!(CapturedEvalContext);

use lazy_static::lazy_static;

lazy_static! {
  pub static ref EVAL_CONTEXT_TYPE: Type = Type::object(FRAG_CC, fourCharacterCode(*b"shev")); // last letter used as version
  pub static ref EVAL_CONTEXT_TYPE_VEC: Vec<Type> = vec![*EVAL_CONTEXT_TYPE];
  pub static ref EVAL_CONTEXT_VAR_TYPE: Type = Type::context_variable(&EVAL_CONTEXT_TYPE_VEC);
}

pub fn capture_eval_context(env: &EvalEnv) -> ClonedVar {
  // Helper function to gather data from all environments in the chain
  fn gather_data<T, F>(env: &EvalEnv, mut result: T, gather_fn: F) -> T
  where
    F: Fn(&EvalEnv, &mut T),
  {
    // Start with current environment
    gather_fn(env, &mut result);

    // Then recursively process all parent environments
    let mut current = env.parent.as_ref();
    while let Some(parent_lookup) = current {
      let parent = unsafe { &**parent_lookup };
      gather_fn(parent, &mut result);
      current = parent.parent.as_ref();
    }

    result
  }

  // Gather definitions from all environments
  let definitions = gather_data(env, HashMap::new(), |e, result| {
    for (k, v) in &e.definitions {
      // Only insert if not already present (child definitions take precedence)
      if !result.contains_key(k) {
        result.insert(k.clone(), v.without_pointers());
      }
    }
  });

  // Gather extensions from all environments
  let extensions = gather_data(env, HashMap::new(), |e, result| {
    for (k, v) in &e.extensions {
      if !result.contains_key(k) {
        result.insert(k.clone(), v.clone());
      }
    }
  });

  // Gather forbidden functions from all environments
  let forbidden_funcs = gather_data(env, HashSet::new(), |e, result| {
    for k in &e.forbidden_funcs {
      result.insert(k.clone());
    }
  });

  // Gather full namespace - takes precedence from current environment
  let full_namespace = env.full_namespace.clone();

  // Gather macro groups
  let macro_groups = gather_data(env, HashMap::new(), |e, result| {
    for (k, v) in &e.macro_groups {
      if !result.contains_key(k) {
        result.insert(k.clone(), v.without_pointers());
      }
    }
  });

  // Continue gathering remaining data
  let meshes = gather_data(env, HashMap::new(), |e, result| {
    for (k, v) in &e.meshes {
      if !result.contains_key(k) {
        result.insert(k.clone(), v.clone());
      }
    }
  });

  let namespace = env.namespace.clone();

  let replacements = gather_data(env, HashMap::new(), |e, result| {
    for (k, v) in &e.replacements {
      if !result.contains_key(k) {
        result.insert(k.clone(), unsafe { (**v).clone() });
      }
    }
  });

  let rewrite_funcs = gather_data(env, HashMap::new(), |e, result| {
    for (k, v) in &e.rewrite_funcs {
      if !result.contains_key(k) {
        result.insert(k.clone(), v.clone());
      }
    }
  });

  let settings = env.settings.clone(); // Settings from current environment only

  let suffix = env.suffix.clone();

  let suffix_assigned = gather_data(env, HashMap::new(), |e, result| {
    for (k, v) in &e.suffix_assigned {
      if !result.contains_key(k) {
        result.insert(k.clone(), v.clone());
      }
    }
  });

  let traits = gather_data(env, HashMap::new(), |e, result| {
    for (k, v) in &e.traits {
      if !result.contains_key(k) {
        result.insert(k.clone(), v.clone());
      }
    }
  });

  let wires = gather_data(env, HashMap::new(), |e, result| {
    for (k, v) in &e.finalized_wires {
      if !result.contains_key(k) {
        result.insert(k.clone(), v.clone());
      }
    }
  });

  let shards_groups = gather_data(env, HashMap::new(), |e, result| {
    for (k, v) in &e.shards_groups {
      if !result.contains_key(k) {
        result.insert(k.clone(), v.without_pointers());
      }
    }
  });

  let ctx = CapturedEvalContext {
    cached_definition_pointers: HashMap::new(),
    cached_macro_group_pointers: HashMap::new(),
    cached_shards_group_pointers: HashMap::new(),
    definitions,
    extensions,
    forbidden_funcs,
    full_namespace,
    macro_groups,
    meshes,
    namespace,
    replacements,
    rewrite_funcs,
    settings,
    shards_groups,
    wires,
    suffix,
    suffix_assigned,
    traits,
  };
  Var::new_ref_counted(ctx, &*EVAL_CONTEXT_TYPE).into()
}

#[derive(Clone)]
enum SVar {
  Cloned(ClonedVar),
  NotCloned(Var),
}

impl AsRef<Var> for SVar {
  fn as_ref(&self) -> &Var {
    match self {
      SVar::Cloned(v) => &v.0,
      SVar::NotCloned(v) => v,
    }
  }
}

impl AsMut<Var> for SVar {
  fn as_mut(&mut self) -> &mut Var {
    match self {
      SVar::Cloned(v) => &mut v.0,
      SVar::NotCloned(v) => v,
    }
  }
}
fn error_invalid_hex(line_info: LineInfo) -> ShardsError {
  ("Invalid hexadecimal parameter", line_info).into()
}

fn is_compile_time_constant(v: &Value, e: &EvalEnv) -> bool {
  match v {
    Value::Func(f) => {
      if let Some(defined) = find_defined(&f.name, e) {
        match defined {
          Definition::ValueSource(v) => is_compile_time_constant(unsafe { &**v }, e),
          Definition::ValueGenerated(v) => is_compile_time_constant(v, e),
          Definition::Constant(_) => true,
        }
      } else {
        false
      }
    }
    Value::Expr(_) | Value::Identifier(_) | Value::Shard(_) | Value::Shards(_) => false,
    Value::TakeTable(_, _) | Value::TakeSeq(_, _) => false,
    _ => true,
  }
}

fn process_vector_built_in_ints_block<const WIDTH: usize>(
  func: &Function,
  line_info: LineInfo,
  e: &mut EvalEnv,
) -> Result<(), ShardsError> {
  // it's either a Const or a MakeVector in this case

  let (params, len) = get_vec_params::<WIDTH, 16>(func, line_info)?;

  let has_variables = params.iter().any(|x| {
    return !is_compile_time_constant(&x.value, e);
  });

  if !has_variables {
    let value = extract_ints_vector_var::<WIDTH>(len, params, line_info, e)?;
    add_const_shard2(func, value, line_info, e)
  } else {
    let shard = extract_make_ints_shard::<WIDTH>(len, params, line_info, e)?;
    let shard = shard_with_id(shard, e, func);
    e.shards.push(shard);
    Ok(())
  }
}

fn handle_vector_built_in_ints<const WIDTH: usize>(
  func: &Function,
  line_info: LineInfo,
  e: &mut EvalEnv,
) -> Result<Var, ShardsError> {
  let (params, len) = get_vec_params::<WIDTH, 16>(func, line_info)?;
  extract_ints_vector_var::<WIDTH>(len, params, line_info, e)
}

fn extract_make_ints_shard<const WIDTH: usize>(
  len: usize,
  params: &Vec<Param>,
  line_info: LineInfo,
  e: &mut EvalEnv,
) -> Result<AutoShardRef, ShardsError> {
  fn error_requires_number(line_info: LineInfo) -> Result<AutoShardRef, ShardsError> {
    Err(
      (
        "vector built-in function requires a floating point number or identifier parameter",
        line_info,
      )
        .into(),
    )
  }

  let shard = match WIDTH {
    2 => AutoShardRef::create("_MakeInt2", Some(line_info.into())),
    3 => AutoShardRef::create("_MakeInt3", Some(line_info.into())),
    4 => AutoShardRef::create("_MakeInt4", Some(line_info.into())),
    8 => AutoShardRef::create("_MakeInt8", Some(line_info.into())),
    16 => AutoShardRef::create("_MakeInt16", Some(line_info.into())),
    _ => {
      return Err(
        (
          "float vector built-in function requires 2, 3, 4, 8 or 16 parameters",
          line_info,
        )
          .into(),
      )
    }
  }
  .unwrap(); // qed, those shards must exist!

  for i in 0..len {
    let var = match &params[i].value {
      Value::Identifier(_)
      | Value::Number(_)
      | Value::Expr(_)
      | Value::EvalExpr(_)
      | Value::Func(_) => as_var(&params[i].value, line_info, Some(shard.0), e),
      _ => return error_requires_number(line_info),
    }?;
    shard
      .0
      .set_parameter(i as i32, *var.as_ref()) // Type conversion should be handled by the shard!
      .map_err(|err| {
        (
          format!(
            "Error setting parameter for _MakeInt{}, error: {}",
            WIDTH, err
          ),
          line_info,
        )
          .into()
      })?;
  }
  Ok(shard)
}

fn extract_ints_vector_var<const WIDTH: usize>(
  len: usize,
  params: &Vec<Param>,
  line_info: LineInfo,
  e: &mut EvalEnv,
) -> Result<shards::SHVar, ShardsError> {
  let mut vector_values: [SVar; WIDTH] = std::array::from_fn(|_| SVar::NotCloned(Var::default()));

  for i in 0..len {
    vector_values[i] = as_var(&params[i].value, line_info, None, e)?;
  }

  if len == 1 {
    // fill with first value
    for i in 1..WIDTH {
      vector_values[i] = vector_values[0].clone();
    }
  }

  fn to_int(v: &SVar, line_info: LineInfo) -> Result<i32, ShardsError> {
    match TryInto::<i32>::try_into(v.as_ref()) {
      Ok(v) => Ok(v),
      Err(_) => Err(("Argument not an integer", line_info).into()),
    }
  }

  match WIDTH {
    2 => {
      let x: i32 = to_int(&vector_values[0], line_info)?;
      let y: i32 = to_int(&vector_values[1], line_info)?;
      Ok((x, y).into())
    }
    3 => {
      let x: i32 = to_int(&vector_values[0], line_info)?;
      let y: i32 = to_int(&vector_values[1], line_info)?;
      let z: i32 = to_int(&vector_values[2], line_info)?;
      Ok((x, y, z).into())
    }
    4 => {
      let x: i32 = to_int(&vector_values[0], line_info)?;
      let y: i32 = to_int(&vector_values[1], line_info)?;
      let z: i32 = to_int(&vector_values[2], line_info)?;
      let w: i32 = to_int(&vector_values[3], line_info)?;
      Ok((x, y, z, w).into())
    }
    8 => {
      let mut result: [i16; 8] = [0; 8];
      for (i, value) in vector_values.iter().enumerate() {
        match i16::try_from(to_int(value, line_info)?) {
          Ok(int_value) => result[i] = int_value,
          Err(_) => {
            return Err(
              (
                "vector built-in function requires parameters that can be converted to i16",
                line_info,
              )
                .into(),
            );
          }
        }
      }
      let result: &[i16; 8] = &result;
      Ok(result.into())
    }
    16 => {
      let mut result: [i8; 16] = [0; 16];
      for (i, value) in vector_values.iter().enumerate() {
        match i8::try_from(to_int(value, line_info)?) {
          Ok(int_value) => result[i] = int_value,
          Err(_) => {
            return Err(
              (
                "vector built-in function requires parameters that can be converted to i8",
                line_info,
              )
                .into(),
            );
          }
        }
      }
      let result = &result;
      Ok(result.into())
    }
    _ => Err(
      (
        "int vector built-in function requires 2, 3, 4, 8, or 16 parameters",
        line_info,
      )
        .into(),
    ),
  }
}

fn get_vec_params<const WIDTH: usize, const MAX: usize>(
  func: &Function,
  line_info: LineInfo,
) -> Result<(&Vec<Param>, usize), ShardsError> {
  let params = func.params.as_ref().ok_or(
    (
      "vector built-in function requires at least 1 parameter",
      line_info,
    )
      .into(),
  )?;
  let len = params.len();
  if len > 16 {
    return Err(
      (
        "vector built-in function requires at most 16 parameters",
        line_info,
      )
        .into(),
    );
  } else if len != 1 && WIDTH != len {
    return Err(
      (
        "vector built-in function requires 1 or the same number of parameters as the vector width",
        line_info,
      )
        .into(),
    );
  }
  Ok((params, len))
}

fn process_vector_built_in_floats_block<const WIDTH: usize>(
  func: &Function,
  line_info: LineInfo,
  e: &mut EvalEnv,
) -> Result<(), ShardsError> {
  // it's either a Const or a MakeVector in this case

  let (params, len) = get_vec_params::<WIDTH, 16>(func, line_info)?;

  let has_variables = params.iter().any(|x| {
    return !is_compile_time_constant(&x.value, e);
  });

  if !has_variables {
    let value = extract_floats_vector_var::<WIDTH>(len, params, line_info, e)?;
    add_const_shard2(func, value, line_info, e)
  } else {
    let shard = extract_make_floats_shard::<WIDTH>(len, params, line_info, e)?;
    let shard = shard_with_id(shard, e, func);
    e.shards.push(shard);
    Ok(())
  }
}

fn handle_vector_built_in_floats<const WIDTH: usize>(
  func: &Function,
  line_info: LineInfo,
  e: &mut EvalEnv,
) -> Result<Var, ShardsError> {
  let (params, len) = get_vec_params::<WIDTH, 4>(func, line_info)?;
  extract_floats_vector_var::<WIDTH>(len, params, line_info, e)
}

fn extract_make_floats_shard<const WIDTH: usize>(
  len: usize,
  params: &Vec<Param>,
  line_info: LineInfo,
  e: &mut EvalEnv,
) -> Result<AutoShardRef, ShardsError> {
  fn error_requires_number(line_info: LineInfo) -> Result<AutoShardRef, ShardsError> {
    Err(
      (
        "vector built-in function requires a floating point number or identifier parameter",
        line_info,
      )
        .into(),
    )
  }

  let shard = match WIDTH {
    2 => AutoShardRef::create("_MakeFloat2", Some(line_info.into())),
    3 => AutoShardRef::create("_MakeFloat3", Some(line_info.into())),
    4 => AutoShardRef::create("_MakeFloat4", Some(line_info.into())),
    _ => {
      return Err(
        (
          "float vector built-in function requires 2, 3, or 4 parameters",
          line_info,
        )
          .into(),
      )
    }
  }
  .unwrap(); // qed, those shards must exist!

  for i in 0..len {
    let var = match &params[i].value {
      Value::Identifier(_)
      | Value::Number(_)
      | Value::Expr(_)
      | Value::EvalExpr(_)
      | Value::Func(_) => as_var(&params[i].value, line_info, Some(shard.0), e),
      _ => return error_requires_number(line_info),
    }?;
    shard
      .0
      .set_parameter(i as i32, *var.as_ref()) // Type conversion should be handled by the shard!
      .map_err(|err| {
        (
          format!(
            "Error setting parameter for _MakeFloat{}, error: {}",
            WIDTH, err
          ),
          line_info,
        )
          .into()
      })?;
  }
  Ok(shard)
}

fn extract_floats_vector_var<const WIDTH: usize>(
  len: usize,
  params: &Vec<Param>,
  line_info: LineInfo,
  e: &mut EvalEnv,
) -> Result<shards::SHVar, ShardsError> {
  let mut vector_values: [SVar; WIDTH] = std::array::from_fn(|_| SVar::NotCloned(Var::default()));

  for i in 0..len {
    vector_values[i] = as_var(&params[i].value, line_info, None, e)?;
  }

  if len == 1 {
    // fill with first value
    for i in 1..WIDTH {
      vector_values[i] = vector_values[0].clone();
    }
  }

  fn to_float(v: &SVar, line_info: LineInfo) -> Result<f64, ShardsError> {
    match TryInto::<f64>::try_into(v.as_ref()) {
      Ok(v) => Ok(v),
      Err(_) => match TryInto::<i64>::try_into(v.as_ref()) {
        Ok(v) => Ok(v as f64),
        Err(_) => Err(("Argument not a number", line_info).into()),
      },
    }
  }

  match WIDTH {
    2 => {
      let x: f64 = to_float(&vector_values[0], line_info)?;
      let y: f64 = to_float(&vector_values[1], line_info)?;
      Ok((x, y).into())
    }
    3 => {
      let x: f32 = to_float(&vector_values[0], line_info)? as f32;
      let y: f32 = to_float(&vector_values[1], line_info)? as f32;
      let z: f32 = to_float(&vector_values[2], line_info)? as f32;
      Ok((x, y, z).into())
    }
    4 => {
      let x: f32 = to_float(&vector_values[0], line_info)? as f32;
      let y: f32 = to_float(&vector_values[1], line_info)? as f32;
      let z: f32 = to_float(&vector_values[2], line_info)? as f32;
      let w: f32 = to_float(&vector_values[3], line_info)? as f32;
      Ok((x, y, z, w).into())
    }
    _ => Err(
      (
        "float vector built-in function requires 2, 3, or 4 parameters",
        line_info,
      )
        .into(),
    ),
  }
}

fn process_color_built_in_function(
  func: &Function,
  line_info: LineInfo,
  e: &mut EvalEnv,
) -> Result<(), ShardsError> {
  let (params, len) = get_vec_params::<4, 4>(func, line_info)?;

  let has_variables = params.iter().any(|x| {
    return !is_compile_time_constant(&x.value, e);
  });

  if !has_variables {
    let value = handle_color_built_in(func, line_info)?;
    add_const_shard2(func, value, line_info, e)
  } else {
    let shard = extract_make_colors_shard(len, params, line_info, e)?;
    let shard = shard_with_id(shard, e, func);
    e.shards.push(shard);
    Ok(())
  }
}

fn extract_make_colors_shard(
  len: usize,
  params: &Vec<Param>,
  line_info: LineInfo,
  e: &mut EvalEnv,
) -> Result<AutoShardRef, ShardsError> {
  fn error_requires_number(line_info: LineInfo) -> Result<AutoShardRef, ShardsError> {
    Err(
      (
        "color built-in function requires a number or identifier parameter",
        line_info,
      )
        .into(),
    )
  }

  let shard = AutoShardRef::create("_MakeColor", Some(line_info.into())).unwrap(); // qed, this shard must exist!

  for i in 0..len {
    let var = match &params[i].value {
      Value::Identifier(_)
      | Value::Number(_)
      | Value::Expr(_)
      | Value::EvalExpr(_)
      | Value::Func(_) => as_var(&params[i].value, line_info, Some(shard.0), e),
      _ => return error_requires_number(line_info),
    }?;
    shard
      .0
      .set_parameter(i as i32, *var.as_ref()) // Type conversion should be handled by the shard!
      .map_err(|err| {
        (
          format!("Error setting parameter for _MakeColor, error: {}", err),
          line_info,
        )
          .into()
      })?;
  }
  Ok(shard)
}

fn handle_color_built_in(func: &Function, line_info: LineInfo) -> Result<Var, ShardsError> {
  let params = func.params.as_ref().ok_or(
    (
      "color built-in function requires at least 1 parameter",
      line_info,
    )
      .into(),
  )?;
  let len = params.len();
  if len > 4 {
    return Err(
      (
        "color built-in function requires at most 4 parameters",
        line_info,
      )
        .into(),
    );
  }

  let color_int = |n: i64| {
    let n: i32 = n.try_into().map_err(|_| {
      (
        "color built-in function requires a number parameter in range of i32",
        line_info,
      )
        .into()
    })?;
    if n < 0 || n > 255 {
      Err(
        (
          "color built-in function requires a number parameter in range of u8",
          line_info,
        )
          .into(),
      )
    } else {
      Ok(n as u8)
    }
  };

  let color_float = |n: f64| {
    let n = n as f32;
    if n < 0.0 || n > 1.0 {
      Err(
        (
          "color built-in function requires a number parameter in range of 0.0 and 1.0",
          line_info,
        )
          .into(),
      )
    } else {
      Ok((n * 255.0) as u8)
    }
  };

  let mut colors = [255; 4];

  fn error_requires_number(line_info: LineInfo) -> Result<Var, ShardsError> {
    Err(
      (
        "color built-in function requires a number parameter",
        line_info,
      )
        .into(),
    )
  }

  let assign_colors = |n: u32, len: usize, colors: &mut [u8; 4]| match len {
    1 => {
      colors[0] = n as u8;
      colors[1] = 0;
      colors[2] = 0;
      colors[3] = 255;
    }
    2 => {
      colors[0] = (n >> 8) as u8;
      colors[1] = n as u8;
      colors[2] = 0;
      colors[3] = 255;
    }
    3 => {
      colors[0] = (n >> 16) as u8;
      colors[1] = (n >> 8) as u8;
      colors[2] = n as u8;
      colors[3] = 255;
    }
    4 => {
      colors[0] = (n >> 24) as u8;
      colors[1] = (n >> 16) as u8;
      colors[2] = (n >> 8) as u8;
      colors[3] = n as u8;
    }
    _ => {}
  };

  if len == 1 {
    match &params[0].value {
      Value::Number(n) => match n {
        Number::Integer(n) => colors.fill(color_int(*n)?),
        Number::Float(n) => colors.fill(color_float(*n)?),
        Number::Hexadecimal(n) => {
          let n = &n.as_str()[2..];
          let s_len = n.len();
          if s_len > 8 {
            return Err(error_invalid_hex(line_info));
          }
          if let Ok(n) = u32::from_str_radix(n, 16) {
            assign_colors(n, s_len / 2, &mut colors);
          } else {
            return Err(error_invalid_hex(line_info));
          }
        }
      },
      _ => return error_requires_number(line_info),
    }
  } else {
    for i in 0..params.len() {
      colors[i] = match &params[i].value {
        Value::Number(n) => match n {
          Number::Integer(n) => color_int(*n)?,
          Number::Float(n) => color_float(*n)?,
          Number::Hexadecimal(_) => return Err(error_invalid_hex(line_info)),
        },
        _ => return error_requires_number(line_info),
      };
    }
  }

  Ok(Var::color_u8s(colors[0], colors[1], colors[2], colors[3]))
}

fn is_forbidden_func<'a>(name: &'a Identifier, e: &'a EvalEnv) -> bool {
  e.is_forbidden_func(name)
}

fn get_rewrite_func<'a>(
  name: &'a Identifier,
  e: &'a EvalEnv,
) -> Option<&'a Arc<dyn RewriteFunction>> {
  e.get_rewrite_func(name)
}

fn find_current_suffix<'a>(e: &'a EvalEnv) -> Option<&'a RcStrWrapper> {
  e.find_current_suffix()
}

fn find_suffix<'a>(name: &'a RcStrWrapper, e: &'a EvalEnv) -> Option<&'a RcStrWrapper> {
  e.find_suffix(name)
}

fn find_replacement<'a>(name: &'a Identifier, e: &'a EvalEnv) -> Option<&'a Value> {
  e.find_replacement(name)
}

fn find_mesh<'a>(name: &'a Identifier, env: &'a mut EvalEnv) -> Option<&'a mut MeshVar> {
  env.find_mesh(name)
}

fn find_trait<'a>(name: &'a Identifier, env: &'a EvalEnv) -> Option<Var> {
  env.find_trait(name)
}

fn find_wire<'a>(name: &'a Identifier, env: &'a EvalEnv) -> Option<(Var, bool)> {
  env.find_wire(name)
}

fn find_extension<'a>(
  name: &'a Identifier,
  env: &'a mut EvalEnv,
) -> Option<&'a Arc<dyn ShardsExtension>> {
  env.find_extension(name)
}

fn get_program<'a>(env: &'a EvalEnv) -> Option<&'a Program> {
  env.get_program()
}

fn find_shards_group<'a>(name: &'a Identifier, e: &'a EvalEnv) -> Option<&'a ShardsGroup> {
  e.find_shards_group(name)
}

fn find_macro_group<'a>(name: &'a Identifier, e: &'a EvalEnv) -> Option<&'a ShardsGroup> {
  e.find_macro_group(name)
}

fn find_defined<'a>(name: &'a Identifier, e: &'a EvalEnv) -> Option<&'a Definition> {
  e.find_defined(name)
}

fn find_replacement_identifier<'a>(
  name: &'a Identifier,
  line_info: LineInfo,
  env: &'a EvalEnv,
) -> Result<Option<&'a Identifier>, ShardsError> {
  if let Some(replacement) = find_replacement(name, env) {
    match replacement {
      Value::Identifier(name) => Ok(Some(name)),
      _ => Err(("Replacement must be an identifier", line_info).into()),
    }
  } else {
    Ok(None)
  }
}

fn finalize_wire(
  wire: &Wire,
  name: &Identifier,
  params: &WireParams,
  line_info: LineInfo,
  env: &mut EvalEnv,
) -> Result<(), ShardsError> {
  let (name, _) = get_full_name(name, env, line_info, name.namespaces.is_empty())?;

  shlog_trace!("Finalizing wire {}", name);

  let param_helper = match &params {
    WireParams::Source(params) => ParamHelper::new(unsafe { &**params }),
    WireParams::Generated(params) => ParamHelper::new(&params),
  };

  // ignore first parameter, which is the name

  let mut sub_env = param_helper
    .get_param_by_name_or_index("Shards", 1)
    .map(|param| match &param.value {
      Value::Shards(seq) => eval_sequence(&seq, Some(env), new_cancellation_token()),
      _ => Err(("Shards parameter must be shards", line_info).into()),
    })
    .ok_or(("Wire must have a Shards parameter", line_info).into())??;
  finalize_env(&mut sub_env)?;
  for shard in sub_env.shards.drain(..) {
    wire.add_shard(shard.0);
  }

  let traits = param_helper
    .get_param_by_name_or_index("Traits", 2)
    .map(|param| match &param.value {
      Value::Seq(s) => Ok(s.clone()),
      _ => Err(("Traits parameter must be a sequence", line_info).into()),
    })
    .unwrap_or(Ok(Vec::new()))?;
  let mut s = AutoSeqVar::new();
  for value in traits {
    let v = as_var(&value, line_info, None, env)?;
    if v.as_ref().valueType != SHType_Trait {
      return Err(
        (
          format!(
            "Traits parameter must be a sequence of traits ({:?} is invalid)",
            value
          ),
          line_info,
        )
          .into(),
      );
    }
    s.0.push(v.as_ref());
  }
  wire.set_traits(unsafe { s.0 .0.payload.__bindgen_anon_1.seqValue });

  let looped = param_helper
    .get_param_by_name_or_index("Looped", 3)
    .map(|param| match &param.value {
      Value::Boolean(b) => Ok(*b),
      _ => Err(("Looped parameter must be a boolean", line_info).into()),
    })
    .unwrap_or(Ok(false))?;
  wire.set_looped(looped);

  if env.settings.iter().any(|&s| s.disallow_impure_wires) {
    wire.set_pure(true);
  } else {
    let pure = param_helper
      .get_param_by_name_or_index("Pure", 4)
      .map(|param| match &param.value {
        Value::Boolean(b) => Ok(*b),
        _ => Err(("Pure parameter must be a boolean", line_info).into()),
      })
      .unwrap_or(Ok(false))?;
    wire.set_pure(pure);
  }

  if !env.settings.iter().any(|&s| s.disallow_unsafe) {
    let unsafe_ = param_helper
      .get_param_by_name_or_index("Unsafe", 5)
      .map(|param| match &param.value {
        Value::Boolean(b) => Ok(*b),
        _ => Err(("Unsafe parameter must be a boolean", line_info).into()),
      })
      .unwrap_or(Ok(false))?;
    wire.set_unsafe(unsafe_);
  }

  if !env.settings.iter().any(|&s| s.disallow_custom_stack_sizes) {
    let stack_size = param_helper
      .get_param_by_name_or_index("StackSize", 6)
      .map(|param| match as_var(&param.value, line_info, None, env)? {
        SVar::Cloned(v) => i64::try_from(&v.0)
          .map_err(|_| ("StackSize parameter must be an integer", line_info).into()),
        SVar::NotCloned(v) => i64::try_from(&v)
          .map_err(|_| ("StackSize parameter must be an integer", line_info).into()),
      })
      .unwrap_or(Ok(MIN_STACK_SIZE))?;

    // ensure stack size is a multiple of 4 and minimum 1024 bytes
    let stack_size = if stack_size < MIN_STACK_SIZE {
      MIN_STACK_SIZE
    } else if stack_size % 4 != 0 {
      stack_size + 4 - (stack_size % 4)
    } else {
      stack_size
    };
    wire.set_stack_size(stack_size as usize);
  }

  let priority = param_helper
    .get_param_by_name_or_index("Priority", 7)
    .map(|param| match &param.value {
      Value::Number(n) => match n {
        Number::Integer(n) => Ok(*n),
        _ => Err(("Priority parameter must be an integer", line_info).into()),
      },
      _ => Err(("Priority parameter must be an integer", line_info).into()),
    })
    .unwrap_or(Ok(0))?;
  wire.set_priority(priority.clamp(i32::MIN as i64, i32::MAX as i64) as i32);

  Ok(())
}

fn finalize_env(env: &mut EvalEnv) -> Result<(), ShardsError> {
  for wire in &env.deferred_wires {
    env
      .finalized_wires
      .insert(wire.0.clone(), wire.1 .0 .0.into());
  }
  for (name, (wire, params, line_info)) in env.deferred_wires.drain().collect::<Vec<_>>() {
    finalize_wire(&wire, &name, &params, line_info, env)?;
  }
  Ok(())
}

fn eval_eval_expr(seq: &Sequence, env: &mut EvalEnv) -> Result<(ClonedVar, LineInfo), ShardsError> {
  let mut sub_env = eval_sequence(seq, Some(env), new_cancellation_token())?;
  if !sub_env.shards.is_empty() {
    let line_info = sub_env.shards[0].0.get_line_info();
    // create an ephemeral wire, execute and grab result
    let wire = Wire::new("eval-ephemeral");
    wire.set_stack_size(EVAL_STACK_SIZE);
    finalize_env(&mut sub_env)?;
    for shard in sub_env.shards.drain(..) {
      wire.add_shard(shard.0);
    }
    let mut mesh = Mesh::default();
    if let Err(e) = mesh.compose(wire.0) {
      return Err(
        (
          e,
          LineInfo {
            line: line_info.0,
            column: line_info.1,
            file: line_info.2,
          },
        )
          .into(),
      );
    }
    mesh.schedule(wire.0, false);

    loop {
      mesh.tick();
      if mesh.is_empty() {
        break;
      }
    }

    let info = wire.get_info();
    if info.failed {
      let msg = std::str::from_utf8(unsafe {
        from_raw_parts_allow_null(
          info.failureMessage.string as *const u8,
          info.failureMessage.len as usize,
        )
      })
      .unwrap(); // should be valid utf8
      Err(
        (
          msg,
          LineInfo {
            line: line_info.0,
            column: line_info.1,
            file: line_info.2,
          },
        )
          .into(),
      )
    } else {
      let result = unsafe { *info.finalOutput };
      Ok((
        result.into(),
        LineInfo {
          line: line_info.0,
          column: line_info.1,
          file: line_info.2,
        },
      ))
    }
  } else {
    Ok((ClonedVar(Var::default()), LineInfo::default()))
  }
}

pub(crate) fn eval_sequence_inline(
  seq: &Sequence,
  env: &mut EvalEnv,
  cancellation_token: Arc<AtomicBool>,
) -> Result<(), ShardsError> {
  for stmt in &seq.statements {
    eval_statement(stmt, env, cancellation_token.clone())?;
  }
  Ok(())
}

pub(crate) fn eval_sequence(
  seq: &Sequence,
  parent: Option<&mut EvalEnv>,
  cancellation_token: Arc<AtomicBool>,
) -> Result<EvalEnv, ShardsError> {
  let mut sub_env = EvalEnv::new(None, parent.map(|p| p as *const EvalEnv), None);
  // remove previous error state
  seq.custom_state.remove::<ShardsError>();
  eval_sequence_inline(seq, &mut sub_env, cancellation_token).map_err(|e| {
    // set error state if there was an error
    seq.custom_state.set(e.clone());
    // and pass it through
    e
  })?;
  Ok(sub_env)
}

fn create_take_table_chain(
  var_name: &Identifier,
  path: &Vec<RcStrWrapper>,
  line: LineInfo,
  e: &mut EvalEnv,
) -> Result<(), ShardsError> {
  add_get_shard(var_name, line, e)?;
  for path_part in path {
    let s = Var::ephemeral_string(path_part.as_str());
    add_take_shard(var_name, &s, line, e)?;
  }
  Ok(())
}

fn create_take_seq_chain(
  var_name: &Identifier,
  path: &Vec<u32>,
  line: LineInfo,
  e: &mut EvalEnv,
) -> Result<(), ShardsError> {
  add_get_shard(var_name, line, e)?;
  for path_part in path {
    let idx = (*path_part).try_into().unwrap(); // read should have caught this
    add_take_shard(var_name, &idx, line, e)?;
  }
  Ok(())
}

fn combine_namespaces(partial: &RcStrWrapper, fully_qualified: &RcStrWrapper) -> RcStrWrapper {
  if fully_qualified.is_empty() || partial.starts_with("$") {
    return partial.clone();
  }

  let fully_qualified = fully_qualified.split('/').collect::<Vec<_>>();
  let partial = partial.split('/').collect::<Vec<_>>();

  let mut combined = Vec::new();

  // start adding initial parts of fully qualified name we don't know about
  for part in fully_qualified.iter() {
    if partial.len() > 1 && partial.contains(part) {
      // break once we hit a part that is in the partial name
      break;
    }
    combined.push(*part);
  }

  // add the rest of the partial name
  for part in partial.iter() {
    combined.push(*part);
  }

  combined.join("/").into()
}

enum ResolvedVar {
  Constant(SVar),
  Variable(SVar),
}

impl ResolvedVar {
  fn new_variable(sv: SVar) -> Self {
    Self::Variable(sv)
  }
  fn new_const(sv: SVar) -> Self {
    Self::Constant(sv)
  }
  fn into_var(self) -> SVar {
    match self {
      Self::Constant(sv) => sv,
      Self::Variable(sv) => sv,
    }
  }
}

fn as_var(
  value: &Value,
  line_info: LineInfo,
  shard: Option<ShardRef>,
  e: &mut EvalEnv,
) -> Result<SVar, ShardsError> {
  resolve_var(value, line_info, shard, e).map(|v| v.into_var())
}

struct VariableResolver<'e> {
  e: &'e mut EvalEnv,
  resolved_identifiers: HashSet<Identifier>,
}

impl<'e> VariableResolver<'e> {
  fn new(e: &'e mut EvalEnv) -> Self {
    Self {
      e,
      resolved_identifiers: HashSet::new(),
    }
  }

  fn resolve(
    &mut self,
    value: &Value,
    line_info: LineInfo,
    shard: Option<ShardRef>,
  ) -> Result<ResolvedVar, ShardsError> {
    self.resolve_var(value, line_info, shard)
  }

  fn visit_once(&mut self, name: &Identifier) -> bool {
    if self.resolved_identifiers.contains(name) {
      false
    } else {
      self.resolved_identifiers.insert(name.clone());
      true
    }
  }

  fn resolve_var(
    &mut self,
    value: &Value,
    line_info: LineInfo,
    shard: Option<ShardRef>,
  ) -> Result<ResolvedVar, ShardsError> {
    match value {
      Value::None(_) => Ok(ResolvedVar::new_const(SVar::NotCloned(Var::default()))),
      Value::Boolean(value) => Ok(ResolvedVar::new_const(SVar::NotCloned((*value).into()))),
      Value::Identifier(ref name) => {
        if !self.visit_once(name) {
          return Err(
            (
              format!("Recursive variable definition \"{}\"", name),
              line_info,
            )
              .into(),
          );
        }

        // could be wire, trait or mesh as "special" cases
        if let Some(trait_) = find_trait(name, self.e) {
          Ok(ResolvedVar::new_const(SVar::Cloned(trait_.into())))
        } else if let Some((wire, _finalized)) = find_wire(name, self.e) {
          Ok(ResolvedVar::new_const(SVar::Cloned(wire.into())))
        } else if let Some(mesh) = find_mesh(name, self.e) {
          Ok(ResolvedVar::new_const(SVar::NotCloned(mesh.0 .0)))
        } else if let Some(replacement) = find_replacement(name, self.e) {
          if let Value::Identifier(current) = replacement {
            if current.namespaces.is_empty() && current.name.as_str() == name.name.as_str() {
              // prevent infinite recursion
              return qualify_variable(name, line_info, self.e).map(ResolvedVar::new_variable);
            }
          }
          self.resolve_var(&replacement.clone(), line_info, shard) // cloned to make borrow checker happy...
        } else {
          qualify_variable(name, line_info, self.e).map(ResolvedVar::new_variable)
        }
      }
      Value::Enum(prefix, value) => {
        let id = findEnumId(prefix.as_str());
        if let Some(id) = id {
          // decompose bits to split id into vendor and type
          // consider this is how id is composed: int64_t id = (int64_t)vendorId << 32 | typeId;
          let vendor_id = (id >> 32) as i32;
          let type_id = id as i32;
          let info = findEnumInfo(vendor_id, type_id)
            .ok_or((format!("Enum {} not found", prefix), line_info).into())?; // should be valid enum
          for i in 0..info.labels.len {
            let c_str = unsafe { CStr::from_ptr(*info.labels.elements.offset(i as isize)) };
            if value.as_str() == c_str.to_str().unwrap() {
              // should be valid utf8
              // we found the enum value
              let mut enum_var = Var::default();
              enum_var.valueType = SHType_Enum;
              let value = unsafe { *info.values.elements.offset(i as isize) };
              enum_var.payload.__bindgen_anon_1.__bindgen_anon_3.enumValue = value;
              enum_var
                .payload
                .__bindgen_anon_1
                .__bindgen_anon_3
                .enumVendorId = vendor_id;
              enum_var
                .payload
                .__bindgen_anon_1
                .__bindgen_anon_3
                .enumTypeId = type_id;
              return Ok(ResolvedVar::new_const(SVar::NotCloned(enum_var)));
            }
          }
          Err(
            (
              format!("Enum value {}.{} not found", prefix, value),
              line_info,
            )
              .into(),
          )
        } else {
          Err((format!("Enum {} not found", prefix), line_info).into())
        }
      }
      Value::Number(num) => match num {
        Number::Integer(n) => Ok(ResolvedVar::new_const(SVar::NotCloned((*n).into()))),
        Number::Float(n) => Ok(ResolvedVar::new_const(SVar::NotCloned((*n).into()))),
        Number::Hexadecimal(s) => {
          let s = s.as_str();
          let s = &s[2..]; // remove 0x
          let z = u64::from_str_radix(s, 16)
            .map_err(|e| (format!("Invalid hexadecimal number: {}", e), line_info).into())?
            as i64; // read should have caught this
          Ok(ResolvedVar::new_const(SVar::NotCloned(z.into())))
        }
      },
      Value::String(ref s) => {
        let s = Var::ephemeral_string(s.as_str());
        Ok(ResolvedVar::new_const(SVar::Cloned(s.into())))
      }
      Value::Bytes(ref b) => {
        let bytes = b.0.as_ref();
        let bytes = Var::ephemeral_slice(bytes);
        Ok(ResolvedVar::new_const(SVar::Cloned(bytes.into())))
      }
      Value::Float2(ref val) => Ok(ResolvedVar::new_const(SVar::NotCloned(val.into()))),
      Value::Float3(ref val) => Ok(ResolvedVar::new_const(SVar::NotCloned(val.into()))),
      Value::Float4(ref val) => Ok(ResolvedVar::new_const(SVar::NotCloned(val.into()))),
      Value::Int2(ref val) => Ok(ResolvedVar::new_const(SVar::NotCloned(val.into()))),
      Value::Int3(ref val) => Ok(ResolvedVar::new_const(SVar::NotCloned(val.into()))),
      Value::Int4(ref val) => Ok(ResolvedVar::new_const(SVar::NotCloned(val.into()))),
      Value::Int8(ref val) => Ok(ResolvedVar::new_const(SVar::NotCloned(val.into()))),
      Value::Int16(ref val) => Ok(ResolvedVar::new_const(SVar::NotCloned(val.into()))),
      Value::Seq(vec) => {
        let mut seq = AutoSeqVar::new();
        for value in vec {
          let value = as_var(value, line_info, shard, self.e)?;
          seq.0.push(value.as_ref());
        }
        Ok(ResolvedVar::new_const(SVar::Cloned(ClonedVar(seq.leak()))))
      }
      Value::Table(value) => {
        let mut table = AutoTableVar::new();
        for (key, value) in value {
          let mut key = as_var(key, line_info, shard, self.e)?;
          if key.as_ref().is_context_var() {
            // if the key is a context var, we need to convert it to a string
            // this allows us to have nice keys without quotes
            key.as_mut().valueType = SHType_String;
          }
          let value = as_var(value, line_info, shard, self.e)?;
          let key_ref = key.as_ref();
          let value_ref = value.as_ref();
          table.0.insert_fast(*key_ref, value_ref);
        }
        Ok(ResolvedVar::new_const(SVar::Cloned(ClonedVar(
          table.leak(),
        ))))
      }
      Value::Shards(seq) => {
        let mut sub_env = eval_sequence(&seq, Some(self.e), new_cancellation_token())?;

        // ok if we have any suffixed assigned in the above environment, we need to leak them to the current
        for (name, suffix) in sub_env.suffix_assigned.drain() {
          self.e.suffix_assigned.insert(name, suffix);
        }

        finalize_env(&mut sub_env)?;

        let mut seq = AutoSeqVar::new();
        for shard in sub_env.shards.drain(..) {
          let s = shard.0 .0;
          let s: Var = s.into();
          debug_assert!(s.valueType == SHType_ShardRef);
          seq.0.push(&s);
        }
        Ok(ResolvedVar::new_const(SVar::Cloned(ClonedVar(seq.leak()))))
      }
      Value::Shard(shard) => {
        let s = create_shard(shard, line_info, self.e)?;
        let s: Var = s.0 .0.into();
        debug_assert!(s.valueType == SHType_ShardRef);
        Ok(ResolvedVar::new_const(SVar::Cloned(s.into())))
      }
      Value::EvalExpr(seq) => {
        let value = eval_eval_expr(&seq, self.e)?;
        Ok(ResolvedVar::new_const(SVar::Cloned(value.0)))
      }
      Value::Expr(seq) => {
        let start_idx = self.e.shards.len();
        let mut sub_env = eval_sequence(&seq, Some(self.e), new_cancellation_token())?;
        if !sub_env.shards.is_empty() {
          // create a temporary variable to hold the result of the expression
          let tmp_name = nanoid!(16);
          // ensure name starts with a letter
          let tmp_name = format!("t{}", tmp_name);
          // debug info
          let line_info = sub_env.shards[0].0.get_line_info();
          let line_info = LineInfo {
            line: line_info.0,
            column: line_info.1,
            file: line_info.2,
          };
          add_assignment_shard_no_suffix("Ref", &tmp_name, line_info, &mut sub_env)
            .map_err(|e| (format!("{:?}", e), line_info).into())?;
          // wrap into a Sub Shard
          finalize_env(&mut sub_env)?;
          let sub = make_sub_shard(sub_env.shards.drain(..).collect(), line_info)?;
          // add this sub shard before the start of this pipeline!
          self.e.shards.insert(start_idx, sub);
          // now add a get shard to get the temporary at the end of the pipeline
          let mut s = Var::ephemeral_string(&tmp_name);
          s.valueType = SHType_ContextVar;
          Ok(ResolvedVar::new_const(SVar::Cloned(s.into())))
        } else {
          Ok(ResolvedVar::new_const(SVar::NotCloned(().into())))
        }
      }
      Value::TakeTable(var_name, path) => {
        let start_idx = self.e.shards.len();
        let mut sub_env = EvalEnv::new(None, Some(self.e), None);
        create_take_table_chain(var_name, path, line_info, &mut sub_env)?;
        if !sub_env.shards.is_empty() {
          // create a temporary variable to hold the result of the expression
          let tmp_name = nanoid!(16);
          // ensure name starts with a letter
          let tmp_name = format!("t{}", tmp_name);
          add_assignment_shard_no_suffix("Ref", &tmp_name, line_info, &mut sub_env)
            .map_err(|e| (format!("{:?}", e), line_info).into())?;
          // wrap into a Sub Shard
          finalize_env(&mut sub_env)?;
          let sub = make_sub_shard(sub_env.shards.drain(..).collect(), line_info)?;
          // add this sub shard before the start of this pipeline!
          self.e.shards.insert(start_idx, sub);

          // simply return the temporary variable
          let mut s = Var::ephemeral_string(tmp_name.as_str());
          s.valueType = SHType_ContextVar;
          Ok(ResolvedVar::new_const(SVar::Cloned(s.into())))
        } else {
          panic!("TakeTable should always return a shard")
        }
      }
      Value::TakeSeq(var_name, path) => {
        let start_idx = self.e.shards.len();
        let mut sub_env = EvalEnv::new(None, Some(self.e), None);
        create_take_seq_chain(var_name, path, line_info, &mut sub_env)?;
        if !sub_env.shards.is_empty() {
          // create a temporary variable to hold the result of the expression
          let tmp_name = nanoid!(16);
          // ensure name starts with a letter
          let tmp_name = format!("t{}", tmp_name);
          add_assignment_shard_no_suffix("Ref", &tmp_name, line_info, &mut sub_env)
            .map_err(|e| (format!("{:?}", e), line_info).into())?;
          // wrap into a Sub Shard
          finalize_env(&mut sub_env)?;
          let sub = make_sub_shard(sub_env.shards.drain(..).collect(), line_info)?;
          // add this sub shard before the start of this pipeline!
          self.e.shards.insert(start_idx, sub);

          // simply return the temporary variable
          let mut s = Var::ephemeral_string(tmp_name.as_str());
          s.valueType = SHType_ContextVar;
          Ok(ResolvedVar::new_const(SVar::Cloned(s.into())))
        } else {
          panic!("TakeTable should always return a shard")
        }
      }
      Value::Func(func) => {
        // Either evaluates the builtin directly or as an expression whenever it contains expressions
        fn eval_as_const_or_expr<F>(
          f: &Function,
          f_const: F,
          line_info: LineInfo,
          e: &mut EvalEnv,
        ) -> Result<SVar, ShardsError>
        where
          F: FnOnce(&mut EvalEnv) -> Result<SVar, ShardsError>,
        {
          let has_variables = if let Some(params) = &f.params {
            params.iter().any(|x: &Param| {
              return !is_compile_time_constant(&x.value, e);
            })
          } else {
            false
          };
          if has_variables {
            let value2 = Value::Expr {
              0: Sequence {
                statements: vec![Statement::Pipeline {
                  0: Pipeline {
                    blocks: vec![Block {
                      content: BlockContent::Func(f.clone()),
                      line_info: Some(line_info),
                      custom_state: CustomStateContainer::new(),
                    }],
                  },
                }],
                custom_state: CustomStateContainer::new(),
              },
            };
            as_var(&value2, line_info, None, e)
          } else {
            f_const(e)
          }
        }

        match (func.name.name.as_str(), func.name.namespaces.is_empty()) {
          ("color", true) => eval_as_const_or_expr(
            func,
            |_e| Ok(SVar::NotCloned(handle_color_built_in(func, line_info)?)),
            line_info,
            self.e,
          )
          .map(ResolvedVar::new_const),
          ("i2", true) => eval_as_const_or_expr(
            func,
            |e| {
              Ok(SVar::NotCloned(handle_vector_built_in_ints::<2>(
                func, line_info, e,
              )?))
            },
            line_info,
            self.e,
          )
          .map(ResolvedVar::new_const),
          ("i3", true) => eval_as_const_or_expr(
            func,
            |e| {
              Ok(SVar::NotCloned(handle_vector_built_in_ints::<3>(
                func, line_info, e,
              )?))
            },
            line_info,
            self.e,
          )
          .map(ResolvedVar::new_const),
          ("i4", true) => eval_as_const_or_expr(
            func,
            |e| {
              Ok(SVar::NotCloned(handle_vector_built_in_ints::<4>(
                func, line_info, e,
              )?))
            },
            line_info,
            self.e,
          )
          .map(ResolvedVar::new_const),
          ("i8", true) => eval_as_const_or_expr(
            func,
            |e| {
              Ok(SVar::NotCloned(handle_vector_built_in_ints::<8>(
                func, line_info, e,
              )?))
            },
            line_info,
            self.e,
          )
          .map(ResolvedVar::new_const),
          ("i16", true) => eval_as_const_or_expr(
            func,
            |e| {
              Ok(SVar::NotCloned(handle_vector_built_in_ints::<16>(
                func, line_info, e,
              )?))
            },
            line_info,
            self.e,
          )
          .map(ResolvedVar::new_const),
          ("f2", true) => eval_as_const_or_expr(
            func,
            |e| {
              Ok(SVar::NotCloned(handle_vector_built_in_floats::<2>(
                func, line_info, e,
              )?))
            },
            line_info,
            self.e,
          )
          .map(ResolvedVar::new_const),
          ("f3", true) => eval_as_const_or_expr(
            func,
            |e| {
              Ok(SVar::NotCloned(handle_vector_built_in_floats::<3>(
                func, line_info, e,
              )?))
            },
            line_info,
            self.e,
          )
          .map(ResolvedVar::new_const),
          ("f4", true) => eval_as_const_or_expr(
            func,
            |e| {
              Ok(SVar::NotCloned(handle_vector_built_in_floats::<4>(
                func, line_info, e,
              )?))
            },
            line_info,
            self.e,
          )
          .map(ResolvedVar::new_const),
          ("platform", true) => Ok(ResolvedVar::new_const(SVar::Cloned(
            process_platform_built_in().into(),
          ))),
          ("type", true) => process_type(func, line_info, self.e).map(ResolvedVar::new_const),
          ("ast", true) => process_ast(func, line_info, self.e).map(ResolvedVar::new_const),
          ("capture-eval-context", true) => {
            let ctx: ClonedVar = capture_eval_context(self.e);
            Ok(ResolvedVar::new_const(SVar::Cloned(ctx)))
          }
          ("namespace", true) => {
            let namespace = self.e.full_namespace.clone();
            let namespace = namespace.as_str();
            Ok(ResolvedVar::new_const(SVar::Cloned(ClonedVar::from(
              Var::ephemeral_string(namespace),
            ))))
          }
          _ => {
            if let Some(defined_value) = find_defined(&func.name, self.e).map(|x| x.clone()) {
              match defined_value {
                Definition::ValueSource(value) => {
                  let replacement = unsafe { &*value };
                  self.resolve_var(replacement, line_info, shard)
                }
                Definition::ValueGenerated(value) => self.resolve_var(&value, line_info, shard),
                Definition::Constant(var) => Ok(ResolvedVar::new_const(var)),
              }
            } else if let Some(mut shards_env) = process_template(func, line_info, self.e)? {
              // @template
              finalize_env(&mut shards_env)?; // finalize the env

              let mut seq = AutoSeqVar::new();

              // shards
              for shard in shards_env.shards.drain(..) {
                let s: Var = shard.0 .0.into();
                seq.0.push(&s);
              }

              // also move possible other possible things we defined!
              for (name, value) in shards_env.definitions.drain() {
                self.e.definitions.insert(name, value);
              }
              assert_eq!(shards_env.deferred_wires.len(), 0);
              for (name, value) in shards_env.finalized_wires.drain() {
                self.e.finalized_wires.insert(name, value);
              }
              for (name, value) in shards_env.shards_groups.drain() {
                self.e.shards_groups.insert(name, value);
              }
              for (name, value) in shards_env.macro_groups.drain() {
                self.e.macro_groups.insert(name, value);
              }
              for (id, mesh) in shards_env.meshes.drain() {
                self.e.meshes.insert(id, mesh);
              }
              for (id, t) in shards_env.traits.drain() {
                self.e.traits.insert(id, t);
              }

              Ok(ResolvedVar::new_const(SVar::Cloned(ClonedVar(seq.leak()))))
            } else if let Some(ast_json) = process_macro(func, line_info, self.e)? {
              let ast_json: &str = ast_json.as_ref().try_into().map_err(|_| {
                (
                  "macro built-in function Shards should output a Json string",
                  line_info,
                )
                  .into()
              })?;

              thread_local! {
                pub static TMP_VALUE: RefCell<Option<Value>> = RefCell::new(None);
              }

              // in this case we expect the ast to be a value
              let decoded_json: Sequence = serde_json::from_str(ast_json).map_err(|e| {
                (
                  format!(
                    "macro built-in function Shards should return a valid Json string: {}",
                    e
                  ),
                  line_info,
                )
                  .into()
              })?;

              let value = match decoded_json.statements.first() {
                Some(Statement::Pipeline(pipeline)) if pipeline.blocks.len() == 1 => {
                  match &pipeline.blocks[0].content {
                    BlockContent::EvalExpr(seq) => Value::EvalExpr(seq.clone()),
                    BlockContent::Expr(seq) => Value::Expr(seq.clone()),
                    BlockContent::Const(value) => value.clone(),
                    _ => Value::None(()),
                  }
                }
                _ => Value::None(()),
              };
              self.resolve_var(&value, line_info, shard)
            } else if let Some(extension) = find_extension(&func.name, self.e) {
              let v = extension.process_to_var(func, line_info)?;
              Ok(ResolvedVar::new_const(SVar::Cloned(v)))
            } else {
              Err(
                (
                  format!("Undefined function or definition {}", func.name),
                  line_info,
                )
                  .into(),
              )
            }
          }
        }
      }
    }
  }
}

fn resolve_var(
  value: &Value,
  line_info: LineInfo,
  shard: Option<ShardRef>,
  e: &mut EvalEnv,
) -> Result<ResolvedVar, ShardsError> {
  VariableResolver::new(e).resolve(value, line_info, shard)
}

fn qualify_variable(
  name: &Identifier,
  line_info: LineInfo,
  e: &mut EvalEnv,
) -> Result<SVar, ShardsError> {
  let (full_name, _) = get_full_name(name, e, line_info, false)?;
  if let Some(suffix) = find_suffix(&full_name, e) {
    let name = format!("{}{}", full_name, suffix);
    let mut s = Var::ephemeral_string(name.as_str());
    s.valueType = SHType_ContextVar;
    Ok(SVar::Cloned(s.into()))
  } else {
    let mut s = Var::ephemeral_string(full_name.as_str());
    s.valueType = SHType_ContextVar;
    Ok(SVar::Cloned(s.into()))
  }
}

fn process_platform_built_in() -> Var {
  if cfg!(target_os = "android") {
    Var::ephemeral_string("android")
  } else if cfg!(target_os = "ios") {
    Var::ephemeral_string("ios")
  } else if cfg!(target_os = "visionos") {
    Var::ephemeral_string("visionos")
  } else if cfg!(target_os = "emscripten") {
    Var::ephemeral_string("emscripten")
  } else if cfg!(target_os = "windows") {
    Var::ephemeral_string("windows")
  } else if cfg!(target_os = "linux") {
    Var::ephemeral_string("linux")
  } else if cfg!(target_os = "macos") {
    Var::ephemeral_string("macos")
  } else {
    unreachable!("You are running an unknown platform");
  }
}

fn get_full_name<'a>(
  name: &'a Identifier,
  e: &'a mut EvalEnv,
  line_info: LineInfo,
  should_find_replacement: bool,
) -> Result<(RcStrWrapper, bool), ShardsError> {
  let (name, is_replacement) = if should_find_replacement {
    if let Some(replacement) = find_replacement_identifier(name, line_info, e)? {
      (replacement, true)
    } else {
      (name, false)
    }
  } else {
    (name, false)
  };
  if let Some(full_name) = e.qualified_cache.get(name) {
    Ok((full_name.clone(), is_replacement))
  } else if name.namespaces.is_empty() {
    let full_name = combine_namespaces(&name.name, &e.full_namespace);
    e.qualified_cache.insert(name.clone(), full_name.clone());
    Ok((full_name, is_replacement))
  } else {
    let full_name = name.resolve();
    e.qualified_cache.insert(name.clone(), full_name.clone());
    Ok((full_name, is_replacement))
  }
}

fn process_ast(func: &Function, line_info: LineInfo, e: &mut EvalEnv) -> Result<SVar, ShardsError> {
  // ast to json
  //serde_json::to_str(func.params[0].v)
  let first_param = func
    .params
    .as_ref()
    .ok_or(
      (
        "ast built-in function requires at least one parameter",
        line_info,
      )
        .into(),
    )?
    .get(0)
    .ok_or(
      (
        "ast built-in function requires at least one parameter",
        line_info,
      )
        .into(),
    )?;

  // if param is an identifier, we need to resolve it
  let first_param = match &first_param.value {
    Value::Identifier(name) => {
      if let Some(replacement) = find_replacement(name, e) {
        replacement
      } else {
        &first_param.value
      }
    }
    _ => &first_param.value,
  };

  let json = serde_json::to_string(&first_param).map_err(|e| {
    (
      format!("ast built-in function failed to convert to json: {}", e),
      line_info,
    )
      .into()
  })?;

  let s = Var::ephemeral_string(json.as_str());
  Ok(SVar::Cloned(s.into()))
}

fn parse_vendor_or_object_id(
  value: &Value,
  err_msg: &'static str,
  line_info: &LineInfo, // assuming this type, replace with the actual one
) -> Result<i32, ShardsError> {
  // assuming this Error type, replace with the actual one
  match value {
    Value::Number(n) => match n {
      Number::Integer(v) => i32::try_from(*v).map_err(|_| {
        (
          format!("{} failed to parse parameter as integer", err_msg),
          *line_info,
        )
          .into()
      }),
      Number::Hexadecimal(v) => {
        let v = &v[2..];
        i32::from_str_radix(v, 16).map_err(|_| {
          (
            format!("{} failed to parse parameter as hexadecimal", err_msg),
            *line_info,
          )
            .into()
        })
      }
      _ => Err(
        (
          format!(
            "{} requires both parameters as integer or hexadecimal",
            err_msg
          ),
          *line_info,
        )
          .into(),
      ),
    },
    _ => Err((format!("{} failed to parse number", err_msg), *line_info).into()),
  }
}

fn process_type(
  func: &Function,
  line_info: LineInfo,
  e: &mut EvalEnv,
) -> Result<SVar, ShardsError> {
  if let Some(ref params) = func.params {
    let param_helper = ParamHelper::new(params);

    let type_ = param_helper.get_param_by_name_or_index("Type", 0).ok_or(
      (
        "type built-in function requires a Type parameter",
        line_info,
      )
        .into(),
    )?;

    let is_var = param_helper
      .get_param_by_name_or_index("Variable", 1)
      .map(|param| match &param.value {
        Value::Boolean(b) => Ok(*b),
        _ => Err(("Variable parameter must be a boolean", line_info).into()),
      })
      .unwrap_or(Ok(false))?;

    let input_type = param_helper
      .get_param_by_name_or_index("InputType", 2)
      .map(|param| match &param.value {
        Value::Boolean(b) => Ok(*b),
        _ => Err(("InputType parameter must be a boolean", line_info).into()),
      })
      .unwrap_or(Ok(false))?;

    let mut vendor_id = param_helper
      .get_param_by_name_or_index("ObjectVendor", 3)
      .map(|param| {
        parse_vendor_or_object_id(
          &param.value,
          "type built-in function, when Type::Object",
          &line_info,
        )
      });

    let mut object_id = param_helper
      .get_param_by_name_or_index("ObjectTypeId", 4)
      .map(|param| {
        parse_vendor_or_object_id(
          &param.value,
          "type built-in function, when Type::Object",
          &line_info,
        )
      });

    if let Some(object_name) = param_helper.get_param_by_name_or_index("ObjectName", 5) {
      let object_name = match &object_name.value {
        Value::String(s) => s.as_str(),
        _ => return Err(("Object parameter must be a string", line_info).into()),
      };
      match find_object_type_vendor_type_pair(object_name) {
        Some((ven_id, obj_id)) => {
          vendor_id = Some(Ok(ven_id));
          object_id = Some(Ok(obj_id));
        }
        None => {
          return Err(("Object parameter must be a valid object name", line_info).into());
        }
      }
    }

    let mut type_ = process_type_desc(&type_.value, input_type, line_info, e)?;

    match (vendor_id, object_id) {
        (Some(Ok(vendor_id)), Some(Ok(object_id))) => {
          // fix up the type
            let native_type = unsafe {type_.as_mut().payload.__bindgen_anon_1.typeValue};
            let native_type = unsafe {&mut *native_type};
            if native_type.basicType != SHType_Object {
              return Err(
                (
                  "type built-in function, when Type::Object, requires both ObjectVendor and ObjectTypeId parameters",
                  line_info,
                )
                  .into(),
              )
            }

          native_type.details.object.vendorId = vendor_id;
          native_type.details.object.typeId = object_id;
        },
        (None, None) => {},
        _ => {
          return Err(
            (
              "type built-in function, when Type::Object, requires both ObjectVendor and ObjectTypeId parameters",
              line_info,
            )
              .into(),
          )
        }
      }

    if is_var {
      let inner_type = unsafe { *type_.as_ref().payload.__bindgen_anon_1.typeValue };
      let inner_types = [inner_type];
      Ok(SVar::Cloned(ClonedVar::from(Type::context_variable(
        &inner_types,
      ))))
    } else {
      Ok(type_)
    }
  } else {
    Err(
      (
        "type built-in function requires at least a Type parameter",
        line_info,
      )
        .into(),
    )
  }
}

fn process_type_desc(
  value: &Value,
  input_type: bool,
  line_info: LineInfo,
  env: &mut EvalEnv,
) -> Result<SVar, ShardsError> {
  let type_ = match &value {
    Value::Shards(seq) => {
      // ensure there is a single shard
      // and ensure that shard only has a single output type
      let sub_env = eval_sequence(&seq, None, new_cancellation_token())?;
      if sub_env.shards.len() != 1 {
        return Err(
          (
            "Type Shards parameter must contain a single shard",
            line_info,
          )
            .into(),
        );
      }
      let shard = &sub_env.shards[0].0;
      let types = if !input_type {
        shard.output_types()
      } else {
        shard.input_types()
      };
      if types.len() != 1 {
        return Err(
          (
            "Type Shards parameter must contain a shard with a single connection type",
            line_info,
          )
            .into(),
        );
      }
      Ok(SVar::Cloned(ClonedVar::from(types[0])))
    }
    Value::Enum(_, _) => process_type_enum(&value, line_info),
    Value::Seq(seq) => {
      // iterate all and as_var them, ensure it's a Type Type though

      let mut types = Vec::new(); // actual storage
      for value in seq {
        let value = process_type_desc(value, input_type, line_info, env)?;
        if value.as_ref().valueType != SHType_Type {
          return Err(("Type Seq parameter can only contain Type values", line_info).into());
        }
        types.push(value);
      }

      let mut inner_types = Vec::new(); // actually weak storage
      for inner_type in &types {
        let inner_type = inner_type.as_ref();
        if inner_type.valueType != SHType_Type {
          return Err(("Type Seq parameter can only contain Type values", line_info).into());
        }
        let inner_type = unsafe { &*inner_type.payload.__bindgen_anon_1.typeValue };
        inner_types.push(*inner_type);
      }

      Ok(SVar::Cloned(ClonedVar::from(Type::seq(&inner_types))))
    }
    Value::Table(pairs) => {
      let mut keys = Vec::new(); // actual storage
      let mut types = Vec::new(); // actual storage
      for (key, value) in pairs {
        let key = as_var(key, line_info, None, env)?;
        let value = process_type_desc(value, input_type, line_info, env)?;
        keys.push(key);
        types.push(value);
      }

      // we need to wrap it into a Table Type
      let mut inner_keys = Vec::new(); // actually weak storage
      let mut inner_types = Vec::new(); // actually weak storage
      for (key, value) in keys.iter().zip(types.iter()) {
        let key = key.as_ref();
        inner_keys.push(*key);

        let value = value.as_ref();
        if value.valueType != SHType_Type {
          return Err(
            (
              "Type Table parameter can only contain Type values",
              line_info,
            )
              .into(),
          );
        }
        let value = unsafe { &*value.payload.__bindgen_anon_1.typeValue };
        inner_types.push(*value);
      }

      Ok(SVar::Cloned(ClonedVar::from(Type::table(
        &inner_keys,
        &inner_types,
      ))))
    }
    Value::Func(_) => {
      // just as_var bypass it
      as_var(&value, line_info, None, env)
    }
    Value::Shard(shard) => {
      let s = create_shard(shard, line_info, env)?;
      let types = if !input_type {
        s.0.output_types()
      } else {
        s.0.input_types()
      };
      if types.len() != 1 {
        return Err(
          (
            "Type Shards parameter must contain a shard with a single connection type",
            line_info,
          )
            .into(),
        );
      }
      Ok(SVar::Cloned(ClonedVar::from(types[0])))
    }
    _ => Err(
      (
        "Type parameter can be any of the following: Enum, Seq, Table, Func",
        line_info,
      )
        .into(),
    ),
  }?;
  Ok(type_)
}

fn process_type_enum(value: &Value, line_info: LineInfo) -> Result<SVar, ShardsError> {
  let (prefix, value) = match value {
    Value::Enum(prefix, value) => (prefix, value),
    _ => return Err(("Type Enum parameter must be an Enum", line_info).into()),
  };
  if prefix.as_str() == "Type" {
    match value.as_str() {
      "None" => Ok(SVar::Cloned(ClonedVar::from(common_type::none))),
      "Any" => Ok(SVar::Cloned(ClonedVar::from(common_type::any))),
      "Bool" => Ok(SVar::Cloned(ClonedVar::from(common_type::bool))),
      "Int" => Ok(SVar::Cloned(ClonedVar::from(common_type::int))),
      "Int2" => Ok(SVar::Cloned(ClonedVar::from(common_type::int2))),
      "Int3" => Ok(SVar::Cloned(ClonedVar::from(common_type::int3))),
      "Int4" => Ok(SVar::Cloned(ClonedVar::from(common_type::int4))),
      "Int8" => Ok(SVar::Cloned(ClonedVar::from(common_type::int8))),
      "Int16" => Ok(SVar::Cloned(ClonedVar::from(common_type::int16))),
      "Float" => Ok(SVar::Cloned(ClonedVar::from(common_type::float))),
      "Float2" => Ok(SVar::Cloned(ClonedVar::from(common_type::float2))),
      "Float3" => Ok(SVar::Cloned(ClonedVar::from(common_type::float3))),
      "Float4" => Ok(SVar::Cloned(ClonedVar::from(common_type::float4))),
      "Color" => Ok(SVar::Cloned(ClonedVar::from(common_type::color))),
      "Wire" => Ok(SVar::Cloned(ClonedVar::from(common_type::wire))),
      "Shard" => Ok(SVar::Cloned(ClonedVar::from(common_type::shard))),
      "Bytes" => Ok(SVar::Cloned(ClonedVar::from(common_type::bytes))),
      "String" => Ok(SVar::Cloned(ClonedVar::from(common_type::string))),
      "Image" => Ok(SVar::Cloned(ClonedVar::from(common_type::image))),
      "Audio" => Ok(SVar::Cloned(ClonedVar::from(common_type::audio))),
      "Object" => Ok(SVar::Cloned(ClonedVar::from(common_type::object))),
      _ => Err((format!("Unknown Type enum value {}", value), line_info).into()),
    }
  } else {
    let id = findEnumId(prefix.as_str())
      .ok_or((format!("Enum {} not found", prefix), line_info).into())?;
    let vendor_id = (id >> 32) as i32;
    let type_id = id as i32;
    Ok(SVar::Cloned(ClonedVar::from(Type::enumeration(
      vendor_id, type_id,
    ))))
  }
}

fn add_shard(shard: &Function, line_info: LineInfo, e: &mut EvalEnv) -> Result<(), ShardsError> {
  let s: AutoShardRef = create_shard(shard, line_info, e)?;
  let s = shard_with_id(s, e, shard);
  e.shards.push(s);
  Ok(())
}

fn get_replacement<'a>(shard: &'a Function, e: &'a EvalEnv) -> Option<Function> {
  get_rewrite_func(&shard.name, e).and_then(|rw| rw.rewrite_function(shard))
}

fn create_shard(
  shard: &Function,
  line_info: LineInfo,
  e: &mut EvalEnv,
) -> Result<AutoShardRef, ShardsError> {
  match create_shard_inner(shard, line_info, e) {
    Ok(s) => {
      // clean any previous error
      shard.custom_state.remove::<ShardsError>();
      Ok(s)
    }
    Err(e) => {
      // set the error
      shard.custom_state.set(e.clone());
      Err(e)
    }
  }
}

fn shard_with_id_ex(shard: AutoShardRef, e: &mut EvalEnv, x: DebugPtr) -> AutoShardRef {
  // add an id to the shard
  let id = get_program(e).map(|p| {
    let mut debug_info = p.metadata.debug_info.borrow_mut();
    debug_info.id_counter += 1;
    let id: u64 = debug_info.id_counter;
    debug_info.id_to_functions.insert(id, x);
    id
  });
  if let Some(id) = id {
    unsafe { (*shard.0 .0).id = id };
  }
  shard
}

fn shard_with_id(shard: AutoShardRef, e: &mut EvalEnv, func: &Function) -> AutoShardRef {
  shard_with_id_ex(shard, e, DebugPtr::Function(func))
}

fn shard_with_id_iden(shard: AutoShardRef, e: &mut EvalEnv, iden: &Identifier) -> AutoShardRef {
  shard_with_id_ex(shard, e, DebugPtr::Identifier(iden))
}

fn create_shard_inner(
  shard: &Function,
  line_info: LineInfo,
  e: &mut EvalEnv,
) -> Result<AutoShardRef, ShardsError> {
  if is_forbidden_func(&shard.name, e) {
    return Err((format!("Forbidden shard {}", shard.name.name), line_info).into());
  }

  let mut replacement_storage = None;
  let shard = if let Some(replacement) = get_replacement(shard, e) {
    let stored = replacement_storage.insert(replacement);
    stored
  } else {
    shard
  };

  let s = AutoShardRef::create(shard.name.name.as_str(), Some(line_info.into())).ok_or(
    (
      format!("Shard {} does not exist", shard.name.name.as_str()),
      line_info,
    )
      .into(),
  )?;

  let mut idx = 0i32;
  let mut as_idx = true;
  if let Some(ref params) = shard.params {
    for param in params {
      // Refresh parameter info on each iteration, as parameters can be dynamic (once a parameter is set, it may change others)
      let info = s.0.parameters();

      if let Some(ref name) = param.name {
        as_idx = false;
        let mut found = false;
        for (i, info) in info.iter().enumerate() {
          let param_name = unsafe { CStr::from_ptr(info.name).to_str().unwrap() }; // should be valid
          if param_name == name.as_str() {
            set_shard_parameter(info, e, &param.value, &s, i, line_info)?;
            found = true;
            break;
          }
        }
        if !found {
          let msg = format!("Unknown parameter '{}'", name);
          return Err((msg, line_info).into());
        }
      } else {
        if !as_idx {
          return Err(("Unnamed parameter after named parameter", line_info).into());
        }
        if idx >= info.len() as i32 {
          return Err(
            (
              format!("Too many parameters for shard {}", shard.name.name).to_string(),
              line_info,
            )
              .into(),
          );
        }
        set_shard_parameter(
          &info[idx as usize],
          e,
          &param.value,
          &s,
          idx as usize,
          line_info,
        )?;
      }
      idx += 1;
    }
  }
  Ok(s)
}

fn set_shard_parameter(
  info: &shards::SHParameterInfo,
  env: &mut EvalEnv,
  value: &Value,
  s: &AutoShardRef,
  i: usize,
  line_info: LineInfo,
) -> Result<(), ShardsError> {
  let var_value = as_var(value, line_info, Some(s.0), env).map_err(|e| e)?;
  if info.variableSetter {
    let name = match value {
      Value::Identifier(name) => name,
      _ => {
        return Err(
          (
            format!("Expected a variable identifier, found {:?}", value),
            line_info,
          )
            .into(),
        )
      }
    };

    if var_value.as_ref().valueType != SHType_ContextVar {
      return Err((format!("Expected a variable, found {:?}", value), line_info).into());
    }

    let (full_name, is_replacement) =
      get_full_name(name, env, line_info, name.namespaces.is_empty())?;

    let suffix = if !is_replacement && name.namespaces.is_empty() {
      // suffix is only relevant if we are not a replacement
      find_current_suffix(env)
    } else {
      None
    };

    if let Some(suffix) = suffix {
      // fix up the value to be a suffixed variable if we have a suffix
      let new_name = format!("{}{}", full_name, suffix);
      // also add to suffix_assigned
      env.suffix_assigned.insert(full_name.into(), suffix.clone());
      let mut new_name = Var::ephemeral_string(new_name.as_str());
      new_name.valueType = SHType_ContextVar;
      if let Err(e) = s.0.set_parameter(
        i.try_into().expect("Too many parameters"),
        *new_name.as_ref(),
      ) {
        let param_name = unsafe { CStr::from_ptr(info.name).to_str().unwrap() }; // should be valid
        Err(
          (
            format!("Failed to set parameter '{}' , error: {}", param_name, e),
            line_info,
          )
            .into(),
        )
      } else {
        Ok(())
      }
    } else {
      if let Err(e) = s.0.set_parameter(
        i.try_into().expect("Too many parameters"),
        *var_value.as_ref(),
      ) {
        let param_name = unsafe { CStr::from_ptr(info.name).to_str().unwrap() }; // should be valid
        Err(
          (
            format!("Failed to set parameter '{}', error: {}", param_name, e),
            line_info,
          )
            .into(),
        )
      } else {
        Ok(())
      }
    }
  } else {
    if let Err(e) = s.0.set_parameter(
      i.try_into().expect("Too many parameters"),
      *var_value.as_ref(),
    ) {
      let param_name = unsafe { CStr::from_ptr(info.name).to_str().unwrap() }; // should be valid
      Err(
        (
          format!("Failed to set parameter '{}', error: {}", param_name, e),
          line_info,
        )
          .into(),
      )
    } else {
      Ok(())
    }
  }
}

fn add_const_shard2(
  func: &Function,
  value: Var,
  line_info: LineInfo,
  e: &mut EvalEnv,
) -> Result<(), ShardsError> {
  let shard = AutoShardRef::create("Const", Some(line_info.into())).unwrap(); // qed, Const must exist
  shard
    .0
    .set_parameter(0, value)
    .map_err(|e| (e, line_info).into())?;
  let shard = shard_with_id(shard, e, func);
  e.shards.push(shard);
  Ok(())
}

fn add_const_shard3(value: Var, line_info: LineInfo, e: &mut EvalEnv) -> Result<(), ShardsError> {
  let shard = AutoShardRef::create("Const", Some(line_info.into())).unwrap(); // qed, Const must exist
  shard
    .0
    .set_parameter(0, value)
    .map_err(|e| (e, line_info).into())?;
  e.shards.push(shard);
  Ok(())
}

fn into_get_or_const(
  value: Value,
  line_info: LineInfo,
  e: &mut EvalEnv,
) -> Result<AutoShardRef, ShardsError> {
  match resolve_var(&value, line_info, None, e)? {
    ResolvedVar::Constant(var) => {
      let shard = AutoShardRef::create("Const", Some(line_info.into())).unwrap(); // qed, Const must exist
      shard
        .0
        .set_parameter(0, *var.as_ref())
        .map_err(|e| (format!("{}", e), line_info).into())?;
      Ok(shard)
    }
    ResolvedVar::Variable(var) => {
      let shard = AutoShardRef::create("Get", Some(line_info.into())).unwrap(); // qed, Get must exist
                                                                                // todo - avoid clone
      shard
        .0
        .set_parameter(0, *var.as_ref())
        .map_err(|e| (format!("{}", e), line_info).into())?;
      Ok(shard)
    }
  }
}

fn add_const_shard(value: &Value, line_info: LineInfo, e: &mut EvalEnv) -> Result<(), ShardsError> {
  let shard = match value {
    Value::Identifier(name) => {
      // we might be a replacement though!
      // we need to evaluate the replacement as not everything can be a const
      if let Some(replacement) = find_replacement(name, e) {
        match replacement {
          Value::None(_)
          | Value::Boolean(_)
          | Value::Enum(_, _)
          | Value::Number(_)
          | Value::String(_)
          | Value::Bytes(_)
          | Value::Int2(_)
          | Value::Int3(_)
          | Value::Int4(_)
          | Value::Int8(_)
          | Value::Int16(_)
          | Value::Float2(_)
          | Value::Float3(_)
          | Value::Float4(_)
          | Value::Seq(_)
          | Value::EvalExpr(_)
          | Value::Expr(_)
          | Value::TakeTable(_, _)
          | Value::TakeSeq(_, _)
          | Value::Func(_)
          | Value::Table(_) => {
            let shard = AutoShardRef::create("Const", Some(line_info.into())).unwrap(); // qed, Const must exist
            let value = as_var(&replacement.clone(), line_info, Some(shard.0), e)?;
            shard
              .0
              .set_parameter(0, *value.as_ref())
              .map_err(|e| (format!("{}", e), line_info).into())?;
            Some(shard)
          }
          Value::Identifier(_) => Some(into_get_or_const(replacement.clone(), line_info, e)?),
          Value::Shard(shard) => {
            // add ourselves
            // todo - avoid clone
            Some(create_shard(&shard.clone(), line_info, e)?)
          }
          Value::Shards(seq) => {
            // purely include the ast of the sequence
            let seq = seq.clone(); // todo - avoid clone
            for stmt in &seq.statements {
              eval_statement(stmt, e, new_cancellation_token())?;
            }
            None
          }
        }
      } else {
        Some(into_get_or_const(value.clone(), line_info, e)?)
      }
      .map(|shard| shard_with_id_iden(shard, e, name))
    }
    _ => {
      let shard = AutoShardRef::create("Const", Some(line_info.into())).unwrap(); // qed, Const must exist
      let value = as_var(value, line_info, Some(shard.0), e)?;
      shard
        .0
        .set_parameter(0, *value.as_ref())
        .map_err(|e| (format!("{}", e), line_info).into())?;
      Some(shard)
    }
  };
  if let Some(shard) = shard {
    // we solve above the id assignment
    e.shards.push(shard);
  }
  Ok(())
}

fn make_sub_shard(
  shards: Vec<AutoShardRef>,
  line_info: LineInfo,
) -> Result<AutoShardRef, ShardsError> {
  let shard = AutoShardRef::create("_SubFlow", Some(line_info.into())).unwrap(); // qed, Sub must exist
  let mut seq = AutoSeqVar::new();
  for shard in shards {
    let s = shard.0 .0;
    let s: Var = s.into();
    debug_assert!(s.valueType == SHType_ShardRef);
    seq.0.push(&s);
  }
  shard
    .0
    .set_parameter(0, seq.0 .0.into())
    .map_err(|e| (format!("{}", e), line_info).into())?;
  Ok(shard)
}

fn add_take_shard(
  name: &Identifier,
  target: &Var,
  line_info: LineInfo,
  e: &mut EvalEnv,
) -> Result<(), ShardsError> {
  let shard = AutoShardRef::create("Take", Some(line_info.into())).unwrap(); // qed, Take must exist
  shard
    .0
    .set_parameter(0, *target)
    .map_err(|e| (format!("{}", e), line_info).into())?;
  let shard = shard_with_id_iden(shard, e, name);
  e.shards.push(shard);
  Ok(())
}

fn add_get_shard(name: &Identifier, line: LineInfo, e: &mut EvalEnv) -> Result<(), ShardsError> {
  let shard = AutoShardRef::create("Get", Some(line.into())).unwrap(); // qed, Get must exist
  let (full_name, is_replacement) = get_full_name(name, e, line, name.namespaces.is_empty())?;
  let suffix = if !is_replacement {
    // suffix is only relevant if we are not a replacement
    find_suffix(&full_name, e)
  } else {
    None
  };
  if let Some(suffix) = suffix {
    let name = format!("{}{}", full_name, suffix);
    let name = Var::ephemeral_string(&name);
    shard
      .0
      .set_parameter(0, name)
      .map_err(|e| (format!("{}", e), line).into())?;
  } else {
    let name = Var::ephemeral_string(full_name.as_str());
    shard
      .0
      .set_parameter(0, name)
      .map_err(|e| (format!("{}", e), line).into())?;
  }
  let shard = shard_with_id_iden(shard, e, name);
  e.shards.push(shard);
  Ok(())
}

fn add_get_shard_no_suffix(name: &str, line: LineInfo, e: &mut EvalEnv) -> Result<(), ShardsError> {
  let shard = AutoShardRef::create("Get", Some(line.into())).unwrap(); // qed, Get must exist
  let name = Var::ephemeral_string(name);
  shard
    .0
    .set_parameter(0, name)
    .map_err(|e| (e, line).into())?;
  // so far this is only used by eval expr so we don't add debug info here
  e.shards.push(shard);
  Ok(())
}

fn get_mesh<'a>(
  param: &'a Param,
  find_mesh: impl Fn(&'a Identifier, &'a mut EvalEnv) -> Option<&'a mut MeshVar>,
  e: &'a mut EvalEnv,
  block: &Block,
) -> Result<&'a mut MeshVar, ShardsError> {
  let line_info = get_block_line_info(e, block);
  match &param.value {
    Value::Identifier(name) => match find_mesh(name, e) {
      Some(mesh) => Ok(mesh),
      None => Err(
        (
          "run built-in function requires a valid mesh parameter",
          line_info,
        )
          .into(),
      ),
    },
    _ => Err(
      (
        "run built-in function requires a mesh parameter",
        get_block_line_info(e, block),
      )
        .into(),
    ),
  }
}

fn process_macro(
  func: &Function,
  line_info: LineInfo,
  e: &mut EvalEnv,
) -> Result<Option<ClonedVar>, ShardsError> {
  if let Some(group) = find_macro_group(&func.name, e) {
    let (args, shards) = match &group {
      ShardsGroup::Source(source) => {
        let args = unsafe { &*source.args };
        let shards = unsafe { &*source.shards };
        (args, shards)
      }
      ShardsGroup::Generated(generated) => {
        let args = &generated.args;
        let shards = &generated.shards;
        (args, shards)
      }
    };

    let expected_params = func.params.as_ref().map(|p| p.len()).unwrap_or(0);
    let provided_args = args.len();

    if provided_args != expected_params {
      return Err(
        (
          format!(
            "Macro {} requires {} parameters, but {} were provided",
            func.name.name, expected_params, provided_args
          ),
          line_info,
        )
          .into(),
      );
    }

    let mut eval_env = EvalEnv::new(None, Some(e), None);

    // set a random suffix
    eval_env.suffix = Some(nanoid!(16).into());

    for i in 0..args.len() {
      let arg = &args[i];
      // arg has to be Identifier
      let arg = match arg {
        Value::Identifier(arg) => {
          if arg.namespaces.is_empty() {
            &arg.name
          } else {
            return Err(
              (
                format!(
                  "Shards macro {} identifier parameters should not be namespaced",
                  func.name.name
                ),
                line_info,
              )
                .into(),
            );
          }
        }
        _ => {
          return Err(
            (
              format!(
                "Shards macro {} parameters should be identifiers",
                func.name.name
              ),
              line_info,
            )
              .into(),
          );
        }
      };

      let param = &func.params.as_ref().ok_or(
        (
          format!(
            "Macro {} requires {} parameters",
            func.name.name,
            args.len()
          ),
          line_info,
        )
          .into(),
      )?[i];

      if param.name.is_some() {
        return Err(
          (
            format!(
              "Shards macro {} does not accept named parameters",
              func.name.name
            ),
            line_info,
          )
            .into(),
        );
      }

      // Resolve template arguments, we could be nested
      let value = if let Value::Identifier(id) = &param.value {
        let replacement = find_replacement(id, e);
        if let Some(replacement) = replacement {
          shlog_trace!("Replacing {:?} with {:?}", id, replacement);
          replacement
        } else {
          &param.value
        }
      } else {
        &param.value
      };

      // and add new replacement
      let value_ptr = value as *const _;
      eval_env.replacements.insert(arg.to_owned(), value_ptr);
    }

    // ok so a macro is AST in Shards that we translate into Json and deserialize as AST
    let (ast_json, _) = eval_eval_expr(shards, &mut eval_env)?;
    Ok(Some(ast_json))
  } else {
    Ok(None)
  }
}

fn process_template(
  func: &Function,
  line_info: LineInfo,
  e: &mut EvalEnv,
) -> Result<Option<EvalEnv>, ShardsError> {
  if let Some(group) = find_shards_group(&func.name, e) {
    let (args, shards) = match &group {
      ShardsGroup::Source(source) => {
        let args = unsafe { &*source.args };
        let shards = unsafe { &*source.shards };
        (args, shards)
      }
      ShardsGroup::Generated(generated) => {
        let args = &generated.args;
        let shards = &generated.shards;
        (args, shards)
      }
    };

    if args.len() != func.params.as_ref().map(|params| params.len()).unwrap_or(0) {
      return Err(
        (
          format!(
            "Shards template {} requires {} parameters",
            func.name.name,
            args.len()
          ),
          line_info,
        )
          .into(),
      );
    }

    let mut sub_env = EvalEnv::new(None, Some(e), None);

    // set a random suffix
    sub_env.suffix = Some(nanoid!(16).into());

    for i in 0..args.len() {
      let arg = &args[i];
      // arg has to be Identifier
      let arg = match arg {
        Value::Identifier(arg) => {
          if arg.namespaces.is_empty() {
            &arg.name
          } else {
            return Err(
              (
                format!(
                  "Shards template {} identifier parameters should not be namespaced",
                  func.name.name
                ),
                line_info,
              )
                .into(),
            );
          }
        }
        _ => {
          return Err(
            (
              format!(
                "Shards template {} parameters should be identifiers",
                func.name.name,
              ),
              line_info,
            )
              .into(),
          );
        }
      };

      let param = &func.params.as_ref().ok_or(
        (
          format!(
            "Shards template {} requires {} parameters",
            func.name.name,
            args.len()
          ),
          line_info,
        )
          .into(),
      )?[i];

      if param.name.is_some() {
        return Err(
          (
            format!(
              "Shards template {} does not accept named parameters",
              func.name.name
            ),
            line_info,
          )
            .into(),
        );
      }

      // Resolve template arguments, we could be nested
      let value = if let Value::Identifier(id) = &param.value {
        let replacement = find_replacement(id, e);
        if let Some(replacement) = replacement {
          shlog_trace!("Replacing {:?} with {:?}", id, replacement);
          replacement
        } else {
          &param.value
        }
      } else {
        &param.value
      };

      // and add new replacement
      let value_ptr = value as *const _;
      sub_env.replacements.insert(arg.to_owned(), value_ptr);
    }

    for stmt in &shards.statements {
      eval_statement(stmt, &mut sub_env, new_cancellation_token())?;
    }

    Ok(Some(sub_env))
  } else {
    Ok(None)
  }
}

fn eval_cond_value(
  v: &Value,
  e: &mut EvalEnv,
  line_info: LineInfo,
  cancellation_token: Arc<AtomicBool>,
) -> Result<(), ShardsError> {
  match v {
    Value::Shards(shards) => {
      for stmt in &shards.statements {
        eval_statement(stmt, e, cancellation_token.clone())?;
      }
    }
    Value::Shard(shard) => {
      add_shard(shard, line_info, e)?;
    }
    Value::None(_) => {}
    _ => {
      return Err(
        (
          format!(
            "if built-in function requires a Shards parameter, got {:?}",
            v
          ),
          line_info,
        )
          .into(),
      )
    }
  }
  return Ok(());
}

fn eval_pipeline(
  pipeline: &Pipeline,
  e: &mut EvalEnv,
  cancellation_token: Arc<AtomicBool>,
) -> Result<(), ShardsError> {
  let start_idx = e.shards.len();
  for block in &pipeline.blocks {
    let _ = match &block.content {
      BlockContent::Empty => Ok(()),
      BlockContent::Shard(shard) => add_shard(shard, get_block_line_info(e, block), e),
      BlockContent::Shards(seq) => {
        let mut sub_env = eval_sequence(&seq, Some(e), cancellation_token.clone())?;

        // if we have a sub env, we need to finalize it
        if !sub_env.shards.is_empty() {
          // sub shards leaks assignments to the parent env
          for name in sub_env.suffix_assigned.drain() {
            e.suffix_assigned.insert(name.0, name.1);
          }
          finalize_env(&mut sub_env)?;
          let sub = make_sub_shard(
            sub_env.shards.drain(..).collect(),
            get_block_line_info(e, block),
          )?;
          // we do not really need to add the sub shard to debug info here, as the errors are likely to be inside
          e.shards.push(sub);
        }

        Ok(())
      }
      BlockContent::Const(value) => add_const_shard(value, get_block_line_info(e, block), e),
      BlockContent::TakeTable(name, path) => {
        create_take_table_chain(name, path, get_block_line_info(e, block), e)
      }
      BlockContent::TakeSeq(name, path) => {
        create_take_seq_chain(name, path, get_block_line_info(e, block), e)
      }
      BlockContent::EvalExpr(seq) => {
        let value = eval_eval_expr(&seq, e)?;
        // we cannot use variation 2 here cos we don't have a func we can use, those blocks will be transformed into a pre-runtime constant
        add_const_shard3(value.0 .0, value.1, e)
      }
      BlockContent::Expr(seq) => {
        eval_expr(seq, e, block, start_idx, cancellation_token.clone())?;
        Ok(())
      }
      BlockContent::Program(p) => {
        // purely include the ast of the sequence
        for stmt in &p.sequence.statements {
          eval_statement(stmt, e, cancellation_token.clone())?;
        }
        Ok(())
      }
      BlockContent::Func(func) => {
        if is_forbidden_func(&func.name, e) {
          return Err(
            (
              format!("Forbidden function {}", func.name),
              get_block_line_info(e, block),
            )
              .into(),
          );
        }

        let mut replacement_storage = None;
        let func = if let Some(replacement) = get_replacement(func, e) {
          let stored = replacement_storage.insert(replacement);
          stored
        } else {
          func
        };

        match (func.name.name.as_str(), func.name.namespaces.is_empty()) {
          ("ignore", true) => {
            // ignore is a special function that does nothing
            Ok(())
          }
          ("trait", true) => {
            if let Some(ref params) = func.params {
              let param_helper = ParamHelper::new(params);

              let name = param_helper.get_param_by_name_or_index("Name", 0).ok_or(
                (
                  "trait built-in function requires Name parameter",
                  get_block_line_info(e, block),
                )
                  .into(),
              )?;

              let types = param_helper.get_param_by_name_or_index("Types", 1).ok_or(
                (
                  "trait built-in function requires Types parameter",
                  get_block_line_info(e, block),
                )
                  .into(),
              )?;

              match (name, types) {
                (
                  Param {
                    value: Value::Identifier(name),
                    ..
                  },
                  types,
                ) => {
                  let make_trait_shards = Sequence {
                    statements: vec![Statement::Pipeline(Pipeline {
                      blocks: vec![Block {
                        content: BlockContent::Shard(Function {
                          name: Identifier {
                            name: RcStrWrapper::from_const("_MakeTrait"),
                            namespaces: vec![],
                            custom_state: CustomStateContainer::new(),
                          },
                          params: Some(vec![
                            Param {
                              name: None,
                              value: Value::String(name.resolve()),
                              custom_state: CustomStateContainer::new(),
                              is_default: None,
                            },
                            types.clone(),
                          ]),
                          custom_state: CustomStateContainer::new(),
                        }),
                        line_info: block.line_info,
                        custom_state: block.custom_state.clone(),
                      }],
                    })],
                    custom_state: CustomStateContainer::new(),
                  };

                  let cvar = eval_eval_expr(&make_trait_shards, e)?;
                  e.traits.insert(name.clone(), cvar.0);

                  Ok(())
                }
                _ => Err(
                  (
                    "trait built-in function requires Name parameter to be an identifier",
                    get_block_line_info(e, block),
                  )
                    .into(),
                ),
              }
            } else {
              Err(
                (
                  "trait built-in function requires proper parameters",
                  get_block_line_info(e, block),
                )
                  .into(),
              )
            }
          }
          ("define", true) => {
            if let Some(ref params) = func.params {
              let param_helper = ParamHelper::new(params);

              let name = param_helper.get_param_by_name_or_index("Name", 0).ok_or(
                (
                  "define built-in function requires Name parameter",
                  get_block_line_info(e, block),
                )
                  .into(),
              )?;

              let value = param_helper.get_param_by_name_or_index("Value", 1).ok_or(
                (
                  "define built-in function requires Value parameter",
                  get_block_line_info(e, block),
                )
                  .into(),
              )?;

              let ignore_redefined = param_helper
                .get_param_by_name_or_index("IgnoreRedefined", 2)
                .map(|v| {
                  if let Value::Boolean(b) = &v.value {
                    *b
                  } else {
                    false
                  }
                })
                .unwrap_or(false);

              let override_def = param_helper
                .get_param_by_name_or_index("Override", 3)
                .map(|v| {
                  if let Value::Boolean(b) = &v.value {
                    *b
                  } else {
                    false
                  }
                })
                .unwrap_or(false);

              match (name, value) {
                (
                  Param {
                    value: Value::Identifier(name),
                    ..
                  },
                  value,
                ) => {
                  if let Some(_) = find_defined(name, e) {
                    if !ignore_redefined && !override_def {
                      return Err(
                        (
                          format!("{} already defined", name.name),
                          get_block_line_info(e, block),
                        )
                          .into(),
                      );
                    } else {
                      if !override_def {
                        // ok we are ignoring redefined, so we just skip
                        return Ok(());
                      }
                    }
                  }

                  // resolve into var if value is a constant
                  if is_compile_time_constant(&value.value, e) {
                    let value = as_var(&value.value, get_block_line_info(e, block), None, e)?;
                    e.definitions
                      .insert(name.clone(), Definition::Constant(value));
                  } else if let ContextType::Generated = e.context_type {
                    e.definitions.insert(
                      name.clone(),
                      Definition::ValueGenerated(value.value.clone()),
                    );
                  } else {
                    e.definitions
                      .insert(name.clone(), Definition::ValueSource(&value.value));
                  }

                  Ok(())
                }
                _ => Err(
                  (
                    "define built-in function requires Name parameter to be an identifier",
                    get_block_line_info(e, block),
                  )
                    .into(),
                ),
              }
            } else {
              Err(
                (
                  "define built-in function requires proper parameters",
                  get_block_line_info(e, block),
                )
                  .into(),
              )
            }
          }
          ("wire", true) => {
            if let Some(ref params) = func.params {
              let param_helper = ParamHelper::new(params);

              let name = param_helper
                .get_param_by_name_or_index("Name", 0)
                .ok_or(
                  (
                    "wire built-in function requires a Name parameter",
                    get_block_line_info(e, block),
                  )
                    .into(),
                )?
                .value
                .get_identifier()
                .ok_or(
                  (
                    "wire built-in function requires a Name parameter",
                    get_block_line_info(e, block),
                  )
                    .into(),
                )?;

              let name = if let Some(replacement) = find_replacement(name, e) {
                replacement
                  .get_identifier()
                  .ok_or(
                    (
                      "wire built-in function requires a Name parameter",
                      get_block_line_info(e, block),
                    )
                      .into(),
                  )?
                  .clone()
              } else {
                name.clone()
              };

              let params_ptr = func.params.as_ref().ok_or(
                (
                  "wire built-in function requires a Params parameter",
                  get_block_line_info(e, block),
                )
                  .into(),
              )? as *const Vec<Param>;
              let (wire_name, _) = get_full_name(
                &name,
                e,
                get_block_line_info(e, block),
                name.namespaces.is_empty(),
              )?;
              shlog_trace!("Adding deferred wire {}", wire_name);
              let params = if let ContextType::Source = e.context_type {
                WireParams::Source(params_ptr)
              } else {
                WireParams::Generated(unsafe { &*params_ptr }.to_vec())
              };
              e.deferred_wires.insert(
                name,
                (
                  Wire::new(&wire_name).set_debug_id({
                    get_program(e)
                      .map(|p| {
                        let mut debug_info = p.metadata.debug_info.borrow_mut();
                        debug_info.id_counter += 1;
                        let id = debug_info.id_counter;
                        debug_info
                          .id_to_functions
                          .insert(id, DebugPtr::Function(func as *const Function));
                        id
                      })
                      .unwrap_or(0)
                  }),
                  params,
                  get_block_line_info(e, block),
                ),
              );
              Ok(())
            } else {
              Err(
                (
                  "wire built-in function requires proper parameters",
                  get_block_line_info(e, block),
                )
                  .into(),
              )
            }
          }
          ("template", true) => {
            if let Some(ref params) = func.params {
              let param_helper = ParamHelper::new(params);

              let name = param_helper.get_param_by_name_or_index("Name", 0).ok_or(
                (
                  "shards built-in function requires a Name parameter",
                  get_block_line_info(e, block),
                )
                  .into(),
              )?;

              let args = param_helper.get_param_by_name_or_index("Args", 1).ok_or(
                (
                  "shards built-in function requires an Args parameter",
                  get_block_line_info(e, block),
                )
                  .into(),
              )?;

              let shards = param_helper.get_param_by_name_or_index("Shards", 2).ok_or(
                (
                  "shards built-in function requires a Shards parameter",
                  get_block_line_info(e, block),
                )
                  .into(),
              )?;

              match (&name.value, &args.value, &shards.value) {
                (Value::Identifier(name), Value::Seq(args), Value::Shards(shards)) => {
                  if let Some(_) = find_shards_group(name, e) {
                    return Err(
                      (
                        format!("template {} already exists", name.name),
                        get_block_line_info(e, block),
                      )
                        .into(),
                    );
                  }

                  let args_ptr = args as *const _;
                  let shards_ptr = shards as *const _;
                  e.shards_groups.insert(
                    name.clone(),
                    if let ContextType::Source = e.context_type {
                      ShardsGroup::Source(ShardsGroupSource {
                        args: args_ptr,
                        shards: shards_ptr,
                      })
                    } else {
                      ShardsGroup::Generated(ShardsGroupGenerated {
                        args: unsafe { &*args_ptr }.clone(),
                        shards: unsafe { &*shards_ptr }.clone(),
                      })
                    },
                  );
                  Ok(())
                }
                _ => Err(
                  (
                    "shards built-in function requires valid parameters",
                    get_block_line_info(e, block),
                  )
                    .into(),
                ),
              }
            } else {
              Err(
                (
                  "shards built-in function requires a Name, Args and Shards parameters",
                  get_block_line_info(e, block),
                )
                  .into(),
              )
            }
          }
          ("mesh", true) => {
            if let Some(ref params) = func.params {
              let param_helper = ParamHelper::new(params);

              let name = param_helper.get_param_by_name_or_index("Name", 0).ok_or(
                (
                  "mesh built-in function requires a name parameter",
                  get_block_line_info(e, block),
                )
                  .into(),
              )?;

              match &name.value {
                Value::Identifier(name) => {
                  if let Some(_) = find_mesh(name, e) {
                    return Err(
                      (
                        format!("mesh {} already exists", name.name),
                        get_block_line_info(e, block),
                      )
                        .into(),
                    );
                  }

                  let mut new_mesh = MeshVar::new();
                  let mesh_name_str: String = name.resolve().to_string();
                  new_mesh.set_label(mesh_name_str.as_str());

                  e.meshes.insert(name.clone(), new_mesh);
                  Ok(())
                }
                _ => Err(
                  (
                    "mesh built-in function requires an identifier parameter",
                    get_block_line_info(e, block),
                  )
                    .into(),
                ),
              }
            } else {
              Err(
                (
                  "mesh built-in function requires a parameter",
                  get_block_line_info(e, block),
                )
                  .into(),
              )
            }
          }
          ("schedule", true) => {
            if let Some(ref _params) = func.params {
              let mut other_fn = func.clone();
              other_fn.name.name = RcStrWrapper::new("Schedule");
              add_shard(&other_fn, get_block_line_info(e, block), e)?;
              Ok(())
            } else {
              Err(
                (
                  "schedule built-in function requires 2 parameters",
                  get_block_line_info(e, block),
                )
                  .into(),
              )
            }
          }
          ("run", true) => {
            if let Some(ref params) = func.params {
              let mut otherFn = func.clone();
              otherFn.name.name = RcStrWrapper::new("Run");
              add_shard(&otherFn, get_block_line_info(e, block), e)?;
              Ok(())
            } else {
              Err(
                (
                  "run built-in function requires a parameter",
                  get_block_line_info(e, block),
                )
                  .into(),
              )
            }
          }
          ("if", true) => {
            if let Some(ref params) = func.params {
              let param_helper = ParamHelper::new(params);
              let v = param_helper.get_param_by_name_or_index("Value", 0).ok_or(
                (
                  "if built-in function requires a Value parameter",
                  get_block_line_info(e, block),
                )
                  .into(),
              )?;
              let then_ = param_helper.get_param_by_name_or_index("Then", 1);
              let else_ = param_helper.get_param_by_name_or_index("Else", 2);

              let v = resolve_var(&v.value, get_block_line_info(e, block), None, e)?.into_var();
              let vr = match &v {
                SVar::Cloned(var) => &var.0,
                SVar::NotCloned(var) => var,
              };

              if !vr.is_bool() {
                return Err(
                  (
                    "if built-in function requires a boolean parameter",
                    get_block_line_info(e, block),
                  )
                    .into(),
                );
              }

              if unsafe { vr.payload.__bindgen_anon_1.boolValue } {
                if let Some(then_) = then_ {
                  eval_cond_value(
                    &then_.value,
                    e,
                    get_block_line_info(e, block),
                    cancellation_token.clone(),
                  )?;
                }
              } else {
                if let Some(else_) = else_ {
                  eval_cond_value(
                    &else_.value,
                    e,
                    get_block_line_info(e, block),
                    cancellation_token.clone(),
                  )?;
                }
              }
              Ok(())
            } else {
              Err(
                (
                  "if built-in function requires a parameter",
                  get_block_line_info(e, block),
                )
                  .into(),
              )
            }
          }
          ("color", true) => {
            process_color_built_in_function(func, get_block_line_info(e, block), e)
          }
          ("i2", true) => {
            process_vector_built_in_ints_block::<2>(func, get_block_line_info(e, block), e)
          }
          ("i3", true) => {
            process_vector_built_in_ints_block::<3>(func, get_block_line_info(e, block), e)
          }
          ("i4", true) => {
            process_vector_built_in_ints_block::<4>(func, get_block_line_info(e, block), e)
          }
          ("i8", true) => {
            process_vector_built_in_ints_block::<8>(func, get_block_line_info(e, block), e)
          }
          ("i16", true) => {
            process_vector_built_in_ints_block::<16>(func, get_block_line_info(e, block), e)
          }
          ("f2", true) => {
            process_vector_built_in_floats_block::<2>(func, get_block_line_info(e, block), e)
          }
          ("f3", true) => {
            process_vector_built_in_floats_block::<3>(func, get_block_line_info(e, block), e)
          }
          ("f4", true) => {
            process_vector_built_in_floats_block::<4>(func, get_block_line_info(e, block), e)
          }
          ("macro", true) => {
            if let Some(ref params) = func.params {
              let param_helper = ParamHelper::new(params);

              let name = param_helper.get_param_by_name_or_index("Name", 0).ok_or(
                (
                  "macro built-in function requires a Name parameter",
                  get_block_line_info(e, block),
                )
                  .into(),
              )?;

              let args = param_helper.get_param_by_name_or_index("Args", 1).ok_or(
                (
                  "macro built-in function requires an Args parameter",
                  get_block_line_info(e, block),
                )
                  .into(),
              )?;

              let shards = param_helper.get_param_by_name_or_index("Shards", 2).ok_or(
                (
                  "macro built-in function requires a Shards parameter",
                  get_block_line_info(e, block),
                )
                  .into(),
              )?;

              match (&name.value, &args.value, &shards.value) {
                (Value::Identifier(name), Value::Seq(args), Value::Shards(shards)) => {
                  let args_ptr = args as *const _;
                  let shards_ptr = shards as *const _;
                  e.macro_groups.insert(
                    name.clone(),
                    if let ContextType::Source = e.context_type {
                      ShardsGroup::Source(ShardsGroupSource {
                        args: args_ptr,
                        shards: shards_ptr,
                      })
                    } else {
                      ShardsGroup::Generated(ShardsGroupGenerated {
                        args: unsafe { &*args_ptr }.clone(),
                        shards: unsafe { &*shards_ptr }.clone(),
                      })
                    },
                  );
                  Ok(())
                }
                _ => Err(
                  (
                    "macro built-in function requires a Name, Args and Shards parameters",
                    get_block_line_info(e, block),
                  )
                    .into(),
                ),
              }
            } else {
              Err(
                (
                  "macro built-in function requires parameters",
                  get_block_line_info(e, block),
                )
                  .into(),
              )
            }
          }
          ("platform", true) => {
            let info = process_platform_built_in();
            add_const_shard2(func, *info.as_ref(), get_block_line_info(e, block), e)
          }
          ("type", true) => {
            let info = process_type(func, get_block_line_info(e, block), e)?;
            add_const_shard2(func, *info.as_ref(), get_block_line_info(e, block), e)
          }
          ("ast", true) => {
            let info = process_ast(func, get_block_line_info(e, block), e)?;
            add_const_shard2(func, *info.as_ref(), get_block_line_info(e, block), e)
          }
          ("eval-context", true) => {
            let ctx = capture_eval_context(e);
            add_const_shard2(func, ctx.0, get_block_line_info(e, block), e)
          }
          ("namespace", true) => {
            let namespace = e.full_namespace.clone();
            add_const_shard2(
              func,
              Var::ephemeral_string(namespace.as_str()),
              get_block_line_info(e, block),
              e,
            )
          }
          _ => {
            let line_info = get_block_line_info(e, block);
            match (
              // Notice, By precedence!
              find_defined(&func.name, e).map(|v| v.clone()),
              process_template(func, line_info, e)?,
              process_macro(func, line_info, e)?,
              find_extension(&func.name, e),
            ) {
              (None, None, None, Some(extension)) => {
                let shard = extension.process_to_shard(func, line_info)?;
                let shard = shard_with_id(shard, e, func);
                e.shards.push(shard);
                Ok(())
              }
              (None, Some(mut shards_env), _, _) => {
                // @template
                finalize_env(&mut shards_env)?; // finalize the env
                                                // shards
                for shard in shards_env.shards.drain(..) {
                  // draining, no need to add id!
                  e.shards.push(shard);
                }
                // also move possible other possible things we defined!
                for (name, value) in shards_env.definitions.drain() {
                  e.definitions.insert(name, value);
                }
                assert_eq!(shards_env.deferred_wires.len(), 0);
                for (name, value) in shards_env.finalized_wires.drain() {
                  e.finalized_wires.insert(name, value);
                }
                for (name, value) in shards_env.shards_groups.drain() {
                  e.shards_groups.insert(name, value);
                }
                for (name, value) in shards_env.macro_groups.drain() {
                  e.macro_groups.insert(name, value);
                }
                for (id, mesh) in shards_env.meshes.drain() {
                  e.meshes.insert(id, mesh);
                }
                for (id, t) in shards_env.traits.drain() {
                  e.traits.insert(id, t);
                }
                Ok(())
              }
              (None, None, Some(ast_json), _) => {
                // macro
                let ast_json: &str = ast_json.as_ref().try_into().map_err(|_| {
                  (
                    "macro built-in function Shards should return a Json string",
                    line_info,
                  )
                    .into()
                })?;

                // in this case we expect the ast to be a sequence of statements
                let decoded_json: Sequence = serde_json::from_str(ast_json).map_err(|err| {
                  (
                    format!(
                      "macro built-in function Shards should return a valid Json string: {}",
                      err
                    ),
                    line_info,
                  )
                    .into()
                })?;

                e.with_line_info(line_info, |e| {
                  e.with_context_mut(ContextType::Generated, |e| {
                    // which we directly evaluate
                    for stmt in &decoded_json.statements {
                      eval_statement(stmt, e, cancellation_token.clone())?;
                    }
                    Ok(())
                  })
                })?;

                Ok(())
              }
              (Some(value), _, _, _) => {
                // defined
                fn eval_def(
                  replacement: &Value,
                  func: &Function,
                  start_idx: usize,
                  block: &Block,
                  e: &mut EvalEnv,
                  cancellation_token: Arc<AtomicBool>,
                ) -> Result<(), ShardsError> {
                  match replacement {
                    Value::None(_)
                    | Value::Identifier(_)
                    | Value::Boolean(_)
                    | Value::Enum(_, _)
                    | Value::Number(_)
                    | Value::String(_)
                    | Value::Bytes(_)
                    | Value::Int2(_)
                    | Value::Int3(_)
                    | Value::Int4(_)
                    | Value::Int8(_)
                    | Value::Int16(_)
                    | Value::Float2(_)
                    | Value::Float3(_)
                    | Value::Float4(_)
                    | Value::Seq(_)
                    | Value::Func(_)
                    | Value::TakeTable(_, _)
                    | Value::TakeSeq(_, _)
                    | Value::Table(_) => {
                      add_const_shard(replacement, get_block_line_info(e, block), e)?
                    }
                    Value::Shards(seq) => {
                      // purely include the ast of the sequence
                      for stmt in &seq.statements {
                        eval_statement(stmt, e, cancellation_token.clone())?;
                      }
                    }
                    Value::EvalExpr(seq) => {
                      let value = eval_eval_expr(&seq, e)?;
                      add_const_shard2(func, value.0 .0, get_block_line_info(e, block), e)?
                    }
                    Value::Expr(seq) => eval_expr(seq, e, block, start_idx, cancellation_token)?,
                    Value::Shard(shard) => add_shard(shard, get_block_line_info(e, block), e)?,
                  }
                  Ok(())
                }
                match value {
                  Definition::Constant(value) => match value {
                    SVar::Cloned(value) => {
                      add_const_shard2(func, value.0, get_block_line_info(e, block), e)?
                    }
                    SVar::NotCloned(value) => {
                      add_const_shard2(func, value, get_block_line_info(e, block), e)?
                    }
                  },
                  Definition::ValueSource(value) => eval_def(
                    unsafe { &*value },
                    func,
                    start_idx,
                    block,
                    e,
                    cancellation_token.clone(),
                  )?,
                  Definition::ValueGenerated(value) => eval_def(
                    &value,
                    func,
                    start_idx,
                    block,
                    e,
                    cancellation_token.clone(),
                  )?,
                }
                Ok(())
              }
              _ => Err(
                (
                  format!("unknown built-in function or definition: {}", func.name),
                  get_block_line_info(e, block),
                )
                  .into(),
              ),
            }
          }
        }
      }
    }?;
  }
  Ok(())
}

fn eval_expr(
  seq: &Sequence,
  e: &mut EvalEnv,
  block: &Block,
  start_idx: usize,
  cancellation_token: Arc<AtomicBool>,
) -> Result<(), ShardsError> {
  let mut sub_env = eval_sequence(&seq, Some(e), cancellation_token)?;
  Ok(if !sub_env.shards.is_empty() {
    // create a temporary variable to hold the result of the expression
    let tmp_name = nanoid!(16);
    // ensure name starts with a letter
    let tmp_name = format!("t{}", tmp_name);
    add_assignment_shard_no_suffix(
      "Ref",
      &tmp_name,
      get_block_line_info(e, block),
      &mut sub_env,
    )
    .map_err(|err| (format!("{:?}", err), get_block_line_info(e, block)).into())?;
    // wrap into a Sub Shard
    finalize_env(&mut sub_env)?;
    let sub = make_sub_shard(
      sub_env.shards.drain(..).collect(),
      get_block_line_info(e, block),
    )?;
    // add this sub shard before the start of this pipeline!
    e.shards.insert(start_idx, sub);
    // now add a get shard to get the temporary at the end of the pipeline
    add_get_shard_no_suffix(&tmp_name, get_block_line_info(e, block), e)?;
  })
}

fn add_assignment_shard(
  shard_name: &str,
  name: &Identifier,
  line_info: LineInfo,
  e: &mut EvalEnv,
) -> Result<(), ShardsError> {
  let shard = AutoShardRef::create(shard_name, Some(line_info.into())).unwrap(); // qed shard_name shard should exist
  let (full_name, is_replacement) = get_full_name(name, e, line_info, name.namespaces.is_empty())?;
  let suffix = if !is_replacement {
    // suffix is only relevant if we are not a replacement
    if shard_name != "Update" && name.namespaces.is_empty() {
      find_current_suffix(e) // this case we add the current suffix
    } else {
      find_suffix(&full_name, e) // this case we want to find a suffix if there is one
    }
  } else {
    None
  };

  let (assigned, suffix) = match (find_replacement(name, e), suffix) {
    (Some(Value::Identifier(name)), _) => {
      let name = name.clone();
      let (full_name, _) = get_full_name(&name, e, line_info, name.namespaces.is_empty())?;
      let name = Var::ephemeral_string(full_name.as_str());
      shard
        .0
        .set_parameter(0, name)
        .map_err(|e| (e, line_info).into())?;
      (None, None)
    }
    (None, Some(suffix)) => {
      let name = format!("{}{}", full_name.as_str(), suffix);
      shard
        .0
        .set_parameter(0, Var::ephemeral_string(&name))
        .map_err(|e| (e, line_info).into())?;
      (Some(full_name.clone()), Some(suffix.clone()))
    }
    (None, None) => {
      let name = Var::ephemeral_string(full_name.as_str());
      shard
        .0
        .set_parameter(0, name)
        .map_err(|e| (e, line_info).into())?;
      (None, None)
    }
    _ => unreachable!(), // Read should prevent this...
  };

  if let Some(name) = assigned {
    e.suffix_assigned.insert(name, suffix.unwrap()); // we know suffix is not none here
  }

  let shard = shard_with_id_iden(shard, e, name);
  e.shards.push(shard);

  Ok(())
}

fn add_assignment_shard_no_suffix(
  shard_name: &str,
  name: &str,
  line_info: LineInfo,
  e: &mut EvalEnv,
) -> Result<(), ShardsError> {
  let shard = AutoShardRef::create(shard_name, Some(line_info.into())).unwrap(); // qed shard_name shard should exist
  let name = Var::ephemeral_string(name);
  shard
    .0
    .set_parameter(0, name)
    .map_err(|e| (e, line_info).into())?;
  // so far this call is used in take seq/table and eval expr, so we don't need to worry debug info
  e.shards.push(shard);
  Ok(())
}

fn eval_assignment(
  assignment: &Assignment,
  e: &mut EvalEnv,
  _cancellation_token: Arc<AtomicBool>,
) -> Result<(), ShardsError> {
  let op = match assignment.kind {
    AssignmentKind::AssignRef => "Ref",
    AssignmentKind::AssignSet => "Set",
    AssignmentKind::AssignUpd => "Update",
    AssignmentKind::AssignPush => "Push",
  };
  // should always have first and always have line info here! (We put Empty type if nothing is there)
  add_assignment_shard(op, &assignment.identifier, assignment.line_info.unwrap(), e)
    .map_err(|e| (format!("{:?}", e), assignment.line_info.unwrap()).into())?;
  Ok(())
}

pub fn eval_statement(
  stmt: &Statement,
  e: &mut EvalEnv,
  cancellation_token: Arc<AtomicBool>,
) -> Result<(), ShardsError> {
  match stmt {
    Statement::Assignment(a) => eval_assignment(a, e, cancellation_token),
    Statement::Pipeline(p) => eval_pipeline(p, e, cancellation_token),
  }
}

pub fn transform_envs<'a, I>(envs: I, name: &str) -> Result<Wire, ShardsError>
where
  I: Iterator<Item = &'a mut EvalEnv>,
{
  let wire = Wire::new(name);
  for env in envs {
    finalize_env(env)?;
    for shard in env.shards.drain(..) {
      wire.add_shard(shard.0);
    }
  }
  Ok(wire)
}

pub fn transform_env(env: &mut EvalEnv, name: &str) -> Result<Wire, ShardsError> {
  let wire = Wire::new(name);
  finalize_env(env)?;
  for shard in env.shards.drain(..) {
    wire.add_shard(shard.0);
  }
  Ok(wire)
}

pub fn merge_env(mut env: EvalEnv, into: &mut EvalEnv) -> Result<(), ShardsError> {
  env.parent = Some(into);

  // finalize here, other wise we will have a namespace mismatch
  finalize_env(&mut env)?;

  for shard in env.shards.drain(..) {
    into.shards.push(shard);
  }

  // also move possible other possible things we defined!
  // the trick here is that we need to decorate identifiers with namespace if they are not
  for (mut name, value) in env.definitions.drain() {
    if name.namespaces.is_empty() {
      name.namespaces.push(env.full_namespace.clone());
    }
    into.definitions.insert(name, value);
  }
  for (mut name, value) in env.deferred_wires.drain() {
    if name.namespaces.is_empty() {
      name.namespaces.push(env.namespace.clone());
    }
    into.deferred_wires.insert(name, value);
  }
  for (mut name, value) in env.finalized_wires.drain() {
    if name.namespaces.is_empty() {
      name.namespaces.push(env.namespace.clone());
    }
    into.finalized_wires.insert(name, value);
  }
  for (mut name, value) in env.shards_groups.drain() {
    if name.namespaces.is_empty() {
      name.namespaces.push(env.namespace.clone());
    }
    into.shards_groups.insert(name, value);
  }
  for (mut name, value) in env.macro_groups.drain() {
    if name.namespaces.is_empty() {
      name.namespaces.push(env.namespace.clone());
    }
    into.macro_groups.insert(name, value);
  }
  for (mut name, mesh) in env.meshes.drain() {
    if name.namespaces.is_empty() {
      name.namespaces.push(env.namespace.clone());
    }
    into.meshes.insert(name, mesh);
  }
  for (mut name, trait_) in env.traits.drain() {
    if name.namespaces.is_empty() {
      name.namespaces.push(env.namespace.clone());
    }
    into.traits.insert(name, trait_);
  }

  Ok(())
}

pub fn eval(
  prog: &Program,
  name: &str,
  defines: HashMap<String, String>,
  cancellation_token: Arc<AtomicBool>,
) -> Result<Wire, ShardsError> {
  profiling::scope!("eval", name);

  let mut parent = EvalEnv::new(None, None, Some(prog as *const Program));
  // add defines
  let defines: Vec<(RcStrWrapper, Value)> = defines
    .iter()
    .map(|(k, v)| {
      (
        k.as_str().to_owned().into(),
        Value::String(v.as_str().to_owned().into()),
      )
    })
    .collect::<Vec<_>>();
  for (name, value) in &defines {
    parent.definitions.insert(
      Identifier {
        name: name.clone(),
        namespaces: Vec::new(),
        custom_state: CustomStateContainer::new(),
      },
      Definition::ValueSource(value),
    );
  }

  let mut env = eval_sequence(
    &prog.sequence,
    Some(&mut parent),
    cancellation_token.clone(),
  )?;

  transform_env(&mut env, name)
}

/// Register an extension which is a type that implements the `ShardsExtension` trait to the environment.
#[allow(dead_code)]
pub fn register_extension<T: ShardsExtension>(ext: Arc<dyn ShardsExtension>, env: &mut EvalEnv) {
  env.extensions.insert(
    Identifier {
      name: ext.name().to_owned().into(),
      namespaces: Vec::new(),
      custom_state: CustomStateContainer::new(),
    },
    ext,
  );
}

lazy_static::lazy_static! {
   // both types are any, as they can be none
   static ref DISTILL_FULL_OUTPUT_TYPES: Vec<Type> = vec![common_type::any, common_type::any, common_type::any];
   static ref DISTILL_FULL_OUTPUT_KEYS: Vec<Var> = vec![
     shards::shstr!("error").into(),
     shards::shstr!("wire").into(),
     shards::shstr!("env").into(),
   ];
   static ref DISTILL_OUTPUT_TYPE: Type =
     Type::table(&DISTILL_FULL_OUTPUT_KEYS, &DISTILL_FULL_OUTPUT_TYPES);
   static ref DISTILL_OUTPUT_TYPES: Vec<Type> = vec![*DISTILL_OUTPUT_TYPE];
}

#[derive(shard)]
#[shard_info("Shards.Distill", "Evaluates a Shards program and outputs a wire.")]
pub struct EvalShard {
  output: ClonedVar,
  #[shard_param("Name", "The optional output wire name.", STRING_VAR_OR_NONE_SLICE)]
  name: ParamVar,
  #[shard_param(
    "Defines",
    "The optional initial injected defines.",
    ANY_TABLE_VAR_NONE_SLICE
  )]
  defines: ParamVar,
  #[shard_param("Namespace", "The optional namespace name.", STRING_VAR_OR_NONE_SLICE)]
  namespace: ParamVar,
  #[shard_param(
    "Context",
    "The evaluation context to inherit templates/definitions/variables from for this script.",
    [common_type::none, *EVAL_CONTEXT_TYPE, *EVAL_CONTEXT_VAR_TYPE]
  )]
  // This is a magic variable that is automatically populated during read
  eval_context: ParamVar,
  #[shard_param(
    "Forbid",
    "The optional forbidden shards and functions.",
    STRINGS_OR_NONE_SLICE
  )]
  forbidden_shards: ClonedVar,
  #[shard_param(
    "FullOutput",
    "Whether to return as output a table with the error and the wire.",
    BOOL_TYPES_SLICE
  )]
  full_output: ClonedVar,
  #[shard_required]
  required: ExposedTypes,
}

impl Default for EvalShard {
  fn default() -> Self {
    Self {
      output: ClonedVar::default(),
      name: ParamVar::default(),
      defines: ParamVar::default(),
      namespace: ParamVar::default(),
      eval_context: ParamVar::default(),
      forbidden_shards: ClonedVar::default(),
      full_output: Var::new_bool(false).into(),
      required: ExposedTypes::new(),
    }
  }
}

#[shard_impl]
impl Shard for EvalShard {
  fn input_types(&mut self) -> &Types {
    &read::AST_TYPES
  }

  fn output_types(&mut self) -> &Types {
    let full_output: bool = unsafe { (&self.full_output.0).try_into().unwrap_unchecked() };
    if !full_output {
      &WIRE_TYPES
    } else {
      &DISTILL_OUTPUT_TYPES
    }
  }

  fn compose(&mut self, data: &InstanceData) -> Result<Type, &str> {
    self.compose_helper(data)?;
    Ok(self.output_types()[0])
  }

  fn warmup(&mut self, context: &Context) -> Result<(), &str> {
    self.warmup_helper(context)?;
    Ok(())
  }

  fn cleanup(&mut self, ctx: Option<&Context>) -> Result<(), &str> {
    self.cleanup_helper(ctx)?;
    Ok(())
  }

  fn activate(&mut self, _: &Context, input: &Var) -> Result<Option<Var>, &str> {
    let maybe_bytes: Result<&[u8], _> = input.try_into();
    let maybe_string: Result<&str, _> = input.try_into();
    let maybe_object = unsafe { Var::from_ref_counted_object::<Program>(input, &AST_TYPE) };

    let eval_context_var = self.eval_context.get();
    let eval_context = if eval_context_var.is_none() {
      None
    } else {
      let eval_context = unsafe {
        &mut *Var::from_ref_counted_object::<CapturedEvalContext>(
          &eval_context_var,
          &*EVAL_CONTEXT_TYPE,
        )?
      };
      Some(eval_context)
    };

    let parent = if let Some(eval_context) = eval_context {
      Some(EvalEnv::from_captured(eval_context))
    } else {
      None
    };
    let parent_ptr = parent.as_ref().map(|p| p as *const EvalEnv);

    let mut prog = match (maybe_bytes, maybe_string, maybe_object) {
      (Ok(bytes), _, _) => {
        // deserialize sequence from bytes
        Some(flexbuffers::from_slice::<Program>(bytes).map_err(|e| {
          shlog_error!("failed to deserialize Shards: {:?}", e);
          "failed to deserialize Shards"
        })?)
      }
      (_, Ok(s), _) => {
        // deserialize sequence from string
        Some(serde_json::from_str(s).map_err(|e| {
          shlog_error!("failed to deserialize Shards: {:?}", e);
          "failed to deserialize Shards"
        })?)
      }
      (_, _, Ok(_)) => None,
      _ => {
        return Err("Invalid input type, expected bytes, string or ast object");
      }
    };

    // ok if we are here we have a valid program, if prog is some, it's either string or bytes
    // if it's none, it's an ast object
    // anyways let's grab a reference now
    let prog = if let Some(ref mut prog) = prog {
      prog
    } else {
      unsafe { &mut (*maybe_object.unwrap()) }
    };

    let namespace = self.namespace.get();
    let mut env = if namespace.is_string() {
      let namespace: &str = namespace.try_into()?;
      EvalEnv::new(
        Some(RcStrWrapper::from(Cow::Owned(namespace.into()))),
        parent_ptr,
        Some(prog as *const Program),
      )
    } else {
      EvalEnv::new(None, parent_ptr, Some(prog as *const Program))
    };

    let defines = self.defines.get();
    let mut defines_storage = Vec::new(); // need to keep this alive until end of activation
    if defines.is_table() {
      let defines = defines.as_table().unwrap(); // qed
      for (ref k, v) in defines.iter() {
        let k: &str = k.try_into()?;
        let v: Value = v.try_into()?;
        defines_storage.push((k, v));
      }
    }
    for (k, v) in &defines_storage {
      env.definitions.insert(
        Identifier {
          name: RcStrWrapper::new(Cow::Owned((*k).into())),
          namespaces: Vec::new(),
          custom_state: CustomStateContainer::new(),
        },
        Definition::ValueSource(v),
      );
    }

    if !self.forbidden_shards.0.is_none() {
      let seq: SeqVar = self.forbidden_shards.0.as_ref().try_into()?;
      for shard in seq.iter() {
        let shard_name: &str = shard.as_ref().try_into()?;
        env.forbidden_funcs.insert(Identifier {
          name: RcStrWrapper::from(Cow::Owned(shard_name.into())),
          namespaces: Vec::new(),
          custom_state: CustomStateContainer::new(),
        });
      }
    }

    let full_output: bool = (&self.full_output.0).try_into()?;
    if !full_output {
      let mut env = eval_sequence(
        &prog.sequence,
        Some(&mut env),
        Arc::new(AtomicBool::new(false)),
      )
      .map_err(|e| {
        shlog_error!("failed to evaluate shards: {:?}", e);
        "failed to evaluate shards"
      })?;

      let name = self.name.get();
      let wire = if name.is_string() {
        let name: &str = name.try_into()?;
        transform_env(&mut env, name).map_err(|e| {
          shlog_error!("failed to transform shards into wire: {:?}", e);
          "failed to transform shards into wire"
        })?
      } else {
        transform_env(&mut env, "_anonymous_wire_").map_err(|e| {
          shlog_error!("failed to transform shards into wire: {:?}", e);
          "failed to transform shards into wire"
        })?
      };

      self.output = wire.0.into();
      Ok(Some(self.output.0))
    } else {
      let mut output_table = AutoTableVar::new();

      let mut env = match eval_sequence(
        &prog.sequence,
        Some(&mut env),
        Arc::new(AtomicBool::new(false)),
      ) {
        Ok(env) => env,
        Err(e) => {
          let error_message = format!("{}, line {}", e.message, e.loc.line);
          let error_var = Var::ephemeral_string(&error_message);
          output_table.0.insert_fast_static("error", &error_var);
          output_table.0.insert_fast_static("wire", &Var::default());
          output_table.0.insert_fast_static("env", &Var::default());
          self.output = output_table.to_cloned();
          return Ok(Some(self.output.0));
        }
      };

      match transform_env(&mut env, "_anonymous_wire_") {
        Ok(wire) => {
          output_table.0.insert_fast_static("error", &Var::default());
          output_table.0.insert_fast_static("wire", &wire.0.into());
          let captured_env = capture_eval_context(&mut env);
          output_table
            .0
            .insert_fast_static("env", &captured_env.0.into());
          self.output = output_table.to_cloned();
          return Ok(Some(self.output.0));
        }
        Err(e) => {
          let error_message = format!("{}, line {}", e.message, e.loc.line);
          let error_var = Var::ephemeral_string(&error_message);
          output_table.0.insert_fast_static("error", &error_var);
          output_table.0.insert_fast_static("wire", &Var::default());
          output_table.0.insert_fast_static("env", &Var::default());
          self.output = output_table.to_cloned();
          return Ok(Some(self.output.0));
        }
      };
    }
  }
}

#[macro_export]
macro_rules! include_shards {
  ($file:expr) => {{
    let code = include_str!($file);
    let successful_parse = ShardsParser::parse(Rule::Program, code).unwrap();
    let mut env = read::ReadEnv::new_cwd("");
    let prog =
      read::process_program(successful_parse.into_iter().next().unwrap(), &mut env).unwrap();
    let defines = std::collections::HashMap::new();
    let token = new_cancellation_token();
    let wire = eval::eval(&prog, "include_shards", defines, token).unwrap();
    let mut mesh = Mesh::default();
    mesh.compose(wire.0).unwrap();
    mesh.schedule(wire.0, false);

    loop {
      mesh.tick();
      if mesh.is_empty() {
        break;
      }
    }

    let info = wire.get_info();
    let result: shards::types::ClonedVar = if info.failed {
      panic!("Failed to evaluate include_shards macro");
    } else {
      unsafe { *info.finalOutput }
    }
    .into();
    result
  }};
}

#[test]
fn test_combine_namespaces() {
  let partial = "b/var";
  let fully_qualified = "a/b";
  let result = combine_namespaces(&partial.into(), &fully_qualified.into());
  assert_eq!(result, RcStrWrapper::from("a/b/var"));

  let partial = "c/d/e/f";
  let fully_qualified = "a/b";
  let result = combine_namespaces(&partial.into(), &fully_qualified.into());
  assert_eq!(result, RcStrWrapper::from("a/b/c/d/e/f"));

  let partial = "b";
  let fully_qualified = "a";
  let result = combine_namespaces(&partial.into(), &fully_qualified.into());
  assert_eq!(result, RcStrWrapper::from("a/b"));

  let partial = "x/b";
  let fully_qualified = "a";
  let result = combine_namespaces(&partial.into(), &fully_qualified.into());
  assert_eq!(result, RcStrWrapper::from("a/x/b"));

  let partial = "b";
  let fully_qualified = "a/b";
  let result = combine_namespaces(&partial.into(), &fully_qualified.into());
  assert_eq!(result, RcStrWrapper::from("a/b/b"));

  let partial = "b/b";
  let fully_qualified = "a/b";
  let result = combine_namespaces(&partial.into(), &fully_qualified.into());
  assert_eq!(result, RcStrWrapper::from("a/b/b"));

  let partial = "b";
  let fully_qualified = "";
  let result = combine_namespaces(&partial.into(), &fully_qualified.into());
  assert_eq!(result, RcStrWrapper::from("b"));

  let partial = "d/g/p";
  let fully_qualified = "a/b";
  let result = combine_namespaces(&partial.into(), &fully_qualified.into());
  assert_eq!(result, RcStrWrapper::from("a/b/d/g/p"));

  let partial = "b/f/g";
  let fully_qualified = "a/b/c/d";
  let result = combine_namespaces(&partial.into(), &fully_qualified.into());
  assert_eq!(result, RcStrWrapper::from("a/b/f/g"));

  let partial = "b/f/c";
  let fully_qualified = "a/b/c/d";
  let result = combine_namespaces(&partial.into(), &fully_qualified.into());
  assert_eq!(result, RcStrWrapper::from("a/b/f/c"));
}
